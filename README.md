El repositorio contiene cuatro sketches independientes:

- src/emitter_turbidez/emitter_turbidez.ino (Nodo TURB, NODE_ID=0)
- src/emitter_ph/emitter_ph.ino (Nodo pH, NODE_ID=2)
- src/emitter_nivel/emitter_nivel.ino (Nodo NIVEL, NODE_ID=1)
- src/central_node/central_node.ino (Nodo CENTRAL, NODE_ID=3)

Todos usan los mismos parámetros LoRa: int state = lora->begin(915.0, 125.0, 7, 5, 0x12, 22, 8, 1.6);
