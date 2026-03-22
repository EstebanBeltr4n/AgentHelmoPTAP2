# Nodo Emisor Turbidez HELMO ID=0

## Calibración experimental
- **Agua limpia**: RAW=1000 → 0 NTU
- **Agua turbia**: RAW=800 → 1000 NTU

## Modelo local
- **EMA**: α=0.3 suavizado temporal
- **Tendencia**: Ventana 3 muestras vs EMA

## LoRa
- 915MHz SF7 CR4/5 → 10km línea vista
- AES-128 payload: "0,245.6,-1.23"
