<#
.SYNOPSIS
    Script to print values of options -tcldir, -outdir, and -arch.

.DESCRIPTION
    This script prompts users for three options (-tcldir, -outdir, and -arch) if not provided and then prints their values.

.PARAMETER tcldir
    Specifies the directory for TCL.
    
.PARAMETER outdir
    Specifies the output directory.
    
.PARAMETER arch
    Specifies the architecture.

.EXAMPLE
    .\Script.ps1 -tcldir C:\TCL -outdir C:\Output -arch x64
    Runs the script with provided options.

#>

param(
    [string]$tcldir = $null,
    [string]$outdir = $null,
    [ValidateSet('x64', 'x86', IgnoreCase = $true)][string]$arch = $null,
    [switch]$help
)


# Define function to prompt for option if not provided
function PromptForOption {
    param(
        [string]$optionName
    )
    $value = Read-Host "Enter value for $optionName"
    return $value
}

if ($help) {
    Get-Help $MyInvocation.MyCommand.Definition -Full
    exit
}

# Prompt for options if not provided
if (-not $tcldir) {
    $tcldir = PromptForOption "-tcldir"
}

if (-not $outdir) {
    $outdir = PromptForOption "-outdir"
}

if (-not $arch) {
    $arch = PromptForOption "-arch"
}

# Print option values
Write-Host "Values:"
Write-Host "-tcldir: $tcldir"
Write-Host "-outdir: $outdir"
Write-Host "-arch: $arch"

# vsdevcmd has a bug that keeps growing the path. Since we read back the path
# amongst other environment variables from vsdevcmd output, save the path so
# we can go back to it after running vsdevcmd. That way our path will not keep
# growing. Otherwise, after 2-3 invocation within the same powershell, we will
# get a command line too long error.
$oldEnv = @{}
#$oldEnv.PATH = $Env:PATH
#$oldEnv.INCLUDE = $Env:INCLUDE
#$oldEnv.EXTERNAL_INCLUDE = $Env:EXTERNAL_INCLUDE
#$oldEnv.LIB = $Env:LIB
#$oldEnv.LIBPATH = $Env:LIBPATH
$envNames = ("PATH", "INCLUDE", "EXTERNAL_INCLUDE", "LIB", "LIBPATH")
foreach ($name in $envNames) {
    if (Test-Path Env:$name) {
        $oldEnv.$name = Get-Content Env:$name
    }
}

$path = tools\vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
# Run vsdevmd to set up environment variabls
if ($path) {
    $path = join-path $path 'Common7\Tools\vsdevcmd.bat'
    if (test-path $path) {
        cmd /s /c """$path"" -arch=$arch && set" | Where-Object { $_ -match '(\w+)=(.*)' } | Foreach-Object {
            $null = new-item -force -path "Env:\$($Matches[1])" -value $Matches[2]
        }
    }
}
Get-ChildItem Env:VS* | Select-Object -Property Name,Value
Push-Location .\win
nmake /s /f makefile.vc INSTALLDIR=$tcldir realclean
nmake /s /f makefile.vc INSTALLDIR=$tcldir
nmake /s /f makefile.vc INSTALLDIR=$tcldir SCRIPT_INSTALL_DIR=$outdir install
Pop-Location
foreach ($e in $oldEnv) {
    Set-Content Env:$e.Name $envNames.$e.Name
}
