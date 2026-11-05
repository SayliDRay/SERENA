$ErrorActionPreference = 'Stop'
$env:TWILIO_ACCOUNT_SID = Read-Host 'Twilio Account SID (AC...)'
$noraCredentialType = Read-Host 'Use Account Auth Token [enter] or API Key [key]'
if ($noraCredentialType -eq 'key') {
    $env:TWILIO_API_KEY_SID = Read-Host 'Twilio Standard API Key SID (SK...)'
    $noraPhoneSecret = Read-Host 'API Key Secret (hidden)' -AsSecureString
    $env:TWILIO_API_KEY_SECRET = [System.Net.NetworkCredential]::new('', $noraPhoneSecret).Password
    Remove-Item Env:TWILIO_AUTH_TOKEN -ErrorAction SilentlyContinue
} else {
    $noraPhoneSecret = Read-Host 'Account Auth Token (hidden)' -AsSecureString
    $env:TWILIO_AUTH_TOKEN = [System.Net.NetworkCredential]::new('', $noraPhoneSecret).Password
    Remove-Item Env:TWILIO_API_KEY_SID, Env:TWILIO_API_KEY_SECRET -ErrorAction SilentlyContinue
}
$env:NORA_TO_NUMBER = Read-Host 'Your verified mobile number (+33...)'
$env:TWILIO_FROM_NUMBER = Read-Host 'From number shown by the successful Twilio Voice test'
$noraPhonePython = Join-Path $PSScriptRoot '../.tools/python-native/python.exe'
try {
    if (Test-Path -LiteralPath $noraPhonePython) {
        & $noraPhonePython (Join-Path $PSScriptRoot 'launch-phone.py') --save-config
    } else {
        python (Join-Path $PSScriptRoot 'launch-phone.py') --save-config
    }
} finally {
    Remove-Item Env:TWILIO_API_KEY_SECRET -ErrorAction SilentlyContinue
    Remove-Item Env:TWILIO_AUTH_TOKEN -ErrorAction SilentlyContinue
}
