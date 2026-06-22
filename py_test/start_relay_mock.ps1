$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $scriptDir

python -c "import config; print('========== Mock Linux_data Relay Test ==========' ); print(f'broker         = {config.MQTT_HOST}:{config.MQTT_PORT}'); print(f'gatewayId      = {config.GATEWAY_ID}'); print(f'portId         = {config.PORT_ID}'); print(f'subscribe cmd  = {config.CMD_TOPIC}'); print(f'subscribe wild = {config.CMD_WILDCARD_TOPIC}'); print(f'publish up     = {config.UP_TOPIC}'); print(f'publish port   = {config.PORT_UP_TOPIC}'); print('==============================================')"
python main.py
