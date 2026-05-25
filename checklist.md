# Rutas Óptimas — Checklist de Implementación

## Estado General

| Fase | Estado |
|------|--------|
| Limpieza de datos (CSV parsing) | ✅ Done |
| Construcción del grafo (adjacency list + auto-reverse) | ✅ Done |
| Smoke test (end-to-end) | ✅ Done |
| Algoritmos de análisis | ❌ Pending |
| Informe / Defensa | ❌ Pending |

---

## Fase 1: Limpieza y Construcción de Datos

- [x] Descargar dataset de Geofabrik (bolivia-latest-free.shp.zip)
- [x] Extraer capa vial desde SHP
- [x] Normalizar atributos: `oneway`, `maxspeed`, `fclass`
- [x] Convertir geometrías a nodos y aristas (coords → node_id)
- [x] Generar `nodes.csv` con `node_id,lat,lon`
- [x] Generar `edges.csv` con `osm_id,from_id,to_id,distance_m,fclass,oneway,maxspeed`
- [x] Descartar geometrías inválidas (null, non-LineString)
- [x] Descartar registros sin coordenadas válidas
- [x] Interpretar `oneway`: 1/0, true/false, yes/no → 1/0
- [x] Interpretar `maxspeed`: "50", "50 km/h", empty → número o 0
- [x] Integrar pipeline de limpieza en csv_parser.h/cpp

---

## Fase 2: Modelado del Grafo

- [x] Estructura `Edge` con: `to`, `weight` (distancia m), `time_weight` (segundos), `maxspeed`, `oneway`
- [x] Clase `Graph` con adjacency list
- [x] Auto-reverse edges para calles bidireccionales (non-oneway)
- [x] Red vehicular dirigida (directed graph)
- [x] Pesos duales: distancia (m) y tiempo (s)
- [x] Inferir velocidad por `fclass` cuando `maxspeed` es 0
- [x] `Graph::build()` — construir desde ParsedNode/ParsedEdge
- [x] `Graph::nodeCount()`, `Graph::edgeCount()`, `Graph::hasNode()`
- [x] Métricas reportadas: 886,790 nodos, 588,485 edges → 1,176,950 directed edges

---

## Fase 3: Algoritmos de Análisis

### 3.1 Alcance Vehicular (5 km)
- [ ] BFS/DFS limitado por distancia acumulada (no euclidiana)
- [ ] Dado nodo origen, contar nodos alcanzables en ≤ 5 km
- [ ] Usar peso `distance_m` para acumulación
- [ ] Reportar: cantidad de nodos alcanzables

### 3.2 Islas Viales (Componentes Débilmente Conexas)
- [ ] DFS/BFS para encontrar componentes débilmente conexas
- [ ] Identificar componente gigante (mayor cantidad de nodos)
- [ ] Reportar: número de islas, tamaño de la componente gigante, grado promedio
- [ ] Comparar con subgrafo filtrado si aplica

### 3.3 Diámetro Vial (Grafo)
- [ ] Encontrar par de nodos con mayor distancia mínima (en metros)
- [ ] Dentro de la componente gigante
- [ ] Algoritmo: BFS desde cada nodo (O(V*(V+E))) o APSP optimizado
- [ ] Reportar: distancia en metros, nodos origen/destino

### 3.4 Red de Emergencia Mínima (MST)
- [ ] Construir MST sobre la componente gigante
- [ ] Algoritmo: Prim o Kruskal
- [ ] Usar peso `distance_m`
- [ ] Reportar: distancia total cubierta en km

### 3.5 (BONUS) Ruta por Tipo de Horario
- [ ] Comparar camino más corto por distancia (d) vs tiempo (t)
- [ ] Dado par de nodos, ejecutar Dijkstra con peso `weight` y con peso `time_weight`
- [ ] Analizar diferencias en ruta resultado

---

## Fase 4: Informe

- [ ] Descripción del problema y modelado
- [ ] Objetivos del proyecto
- [ ] Desarrollo: limpieza, construcción del grafo, retos y complejidad
- [ ] Conclusiones
- [ ] Bibliografía y referencias
- [ ] Comparación de al menos 2 enfoques (ej: Dijkstra vs Bellman-Ford, o distancia vs tiempo)
- [ ] Secciones: Portada, Introducción, Dataset, Formulación Algorítmica, Complejidad, Implementación, Resultados, Conclusiones, Referencias
- [ ] Documentar tamaño de archivo, fecha de descarga, pasos de transformación
- [ ] Métricas de la componente gigante vs grafo completo filtrado
- [ ] Justificar modelado (dirigido/no dirigido, pesos, restricciones)
- [ ] Tiempos reales medidos y comparados por escenario

---

## Fase 5: Defensa Individual

- [ ] Ambos integrantes dominan limpieza de datos
- [ ] Ambos dominan modelado de red vial
- [ ] Ambos dominan algoritmos de caminos más cortos
- [ ] Preparar demo funcional
- [ ] Preparar para preguntas sobre complejidad y decisiones de diseño

---

## Notas de Implementación

### Velocidades inferidas por fclass (km/h)

| fclass | speed_kmh |
|--------|-----------|
| motorway | 100 |
| trunk | 80 |
| primary | 60 |
| secondary | 50 |
| tertiary | 40 |
| residential | 30 |
| service | 20 |
| unclassified | 20 |
| path | 10 |
| track | 10 |
| default | 20 |

### Estructura de archivos

```
include/
  csv_parser.h   ✅  (ParsedNode, ParsedEdge, CSVParser)
  graph.h        ✅  (Edge, Graph)
src/
  csv_parser.cpp ✅
  graph.cpp      ✅
  main.cpp       ✅  (smoke test integration)
data/
  nodes.csv      ✅  (886,790 nodos)
  edges.csv      ✅  (588,485 aristas)
```

### Métricas actuales

| Métrica | Valor |
|---------|-------|
| Nodos | 886,790 |
| Aristas originales | 588,485 |
| Aristas dirigidas (con auto-reverse) | 1,176,950 |