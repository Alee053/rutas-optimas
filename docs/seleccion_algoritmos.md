# Selección de Algoritmos y Justificación Técnica
**Proyecto: Rutas Óptimas en Red Vial Urbana (Bolivia)**

Este documento detalla la selección de algoritmos implementados en el motor C++ para el análisis de la red vial de Bolivia, justificando las decisiones de diseño técnico en base a la complejidad computacional y las características específicas de los datos de OpenStreetMap (886,790 nodos y 1,042,436 aristas dirigidas).

---

## 1. Resumen de Algoritmos Seleccionados

La siguiente tabla resume los algoritmos seleccionados para cada uno de los objetivos del proyecto:

| Objetivo | Algoritmo Seleccionado | Complejidad Teórica | Tiempo Real (Bolivia Dataset) | Justificación Clave |
| :--- | :--- | :--- | :--- | :--- |
| **3.1 Alcance Vehicular (5 km)** | Dijkstra con Cutoff Temprano (Min-Priority Queue) | $O((V_p + E_p) \log V_p)$ | **~4 ms** | Permite detener la exploración inmediatamente al superar la distancia límite, evitando procesar el resto del grafo. |
| **3.2 Componentes Conexas** | DFS Iterativo Bidireccional | $O(V + E)$ | **~200 ms** | Encuentra componentes débilmente conexas en una red dirigida tratando las aristas como no dirigidas, evitando recursión profunda (stack overflow). |
| **3.3 Diámetro Vial** | Dijkstra de Doble Barrido (Double-Sweep Heuristic) | $O(V_{giant} \log V_{giant} + E_{giant})$ | **~200 ms** | El cálculo exacto (APSP) es $O(V^2 \log V)$ e impracticable para $V > 63,000$. El doble barrido estima la cota inferior en tiempo lineal. |
| **3.4 Red de Emergencia (MST)** | Algoritmo de Kruskal con Union-Find | $O(E_{giant} \log E_{giant})$ | **~210 ms** | Más eficiente y simple de implementar sobre listas de adyacencia dispersas que Prim, utilizando compresión de caminos. |
| **3.5 Ruta por Horario (BONUS)** | Dijkstra Dual (Distancia vs. Tiempo) | $O(V \log V + E)$ | **~40 ms** | Algoritmo estándar óptimo para caminos mínimos con pesos no negativos. |

---

## 2. Detalle y Justificación por Algoritmo

### 2.1 Alcance Vehicular (Dijkstra con Cutoff)
* **Descripción**: Dado un nodo origen, encuentra todos los nodos alcanzables a una distancia vial (no euclidiana) $\le 5\text{ km}$.
* **Por qué no BFS simple**: BFS asume que todas las aristas tienen el mismo peso (1). Dado que las calles tienen distancias variables en metros (`distance_m`), se requiere un algoritmo para grafos ponderados.
* **Justificación técnica**: Se implementó una variante de Dijkstra utilizando una cola de prioridad (`std::priority_queue`). Se optimizó introduciendo un *cutoff* temprano: dado que la cola de prioridad extrae los nodos en orden no decreciente de distancia, en el instante en que el nodo extraído supera los $5,000\text{ m}$, el algoritmo rompe el ciclo principal (`break`), garantizando que solo se visite la vecindad local del nodo origen. Esto reduce el subgrafo analizado a $V_p \ll V$.

### 2.2 Componentes Débilmente Conexas (DFS Iterativo Bidireccional)
* **Descripción**: Identifica las "islas viales" (componentes desconectadas) y determina el tamaño del componente gigante.
* **Por qué no DFS recursivo**: El grafo tiene casi 900,000 nodos. Un algoritmo recursivo DFS clásico excedería el límite de la pila de llamadas del sistema operativo (*stack overflow*).
* **Justificación técnica**: Se utiliza un DFS iterativo que mantiene una pila explícita en memoria heap (`std::vector<int> stack`). Para tratar el grafo dirigido como débilmente conexo (donde el sentido de la calle no impide la conexión estructural), el recorrido explora tanto los vecinos salientes (`adj_`) como los vecinos entrantes (calculados en un grafo reverso `rev`). Esto garantiza una partición lineal exacta de todos los nodos en componentes conexas en $O(V + E)$.

### 2.3 Diámetro Vial (Dijkstra de Doble Barrido)
* **Descripción**: Encuentra la mayor distancia mínima entre cualquier par de intersecciones dentro de la componente gigante.
* **Por qué no APSP (All-Pairs Shortest Path)**: Algoritmos como Floyd-Warshall ($O(V^3)$) o Dijkstra para todos los pares ($O(V^2 \log V + VE)$) requerirían trillones de operaciones para los 63,192 nodos de la componente gigante, tardando días.
* **Justificación técnica**: Se utilizó el método heurístico del **doble barrido de Dijkstra** (Double-Sweep):
  1. Se selecciona un nodo arbitrario $s$ en el componente gigante.
  2. Se ejecuta Dijkstra desde $s$ para encontrar el nodo más lejano $u$.
  3. Se ejecuta un segundo Dijkstra desde $u$ para encontrar el nodo más lejano $w$.
  4. La distancia entre $u$ y $w$ se reporta como el diámetro.
  
  Aunque este algoritmo es exacto únicamente para árboles, proporciona una cota inferior extremadamente ajustada y de excelente calidad para redes viales en tiempo lineal ($O(V_{giant} \log V_{giant} + E_{giant})$), tomando solo **0.2 segundos** frente a la imposibilidad práctica del método exacto.

### 2.4 Red de Emergencia Mínima (Kruskal con Union-Find)
* **Descripción**: Construye el árbol de expansión mínima (MST) sobre la componente gigante.
* **Por qué Kruskal y no Prim**: Kruskal procesa un conjunto global de aristas ordenadas, lo cual es muy directo de implementar sobre la componente gigante tras filtrar y deduplicar las aristas paralelas bidireccionales.
* **Justificación técnica**: Se implementó Kruskal utilizando la estructura de datos **Union-Find** (o conjuntos disjuntos) con optimización de compresión de caminos en la búsqueda (`find`). Las aristas se ordenan por su longitud en metros, y se agregan secuencialmente si pertenecen a componentes desconectadas. Esto minimiza el costo total de la red de emergencia a lo largo del componente gigante en $O(E \log E)$.

### 2.5 Comparación de Rutas (Dijkstra Dual con Pesos Distancia vs. Tiempo)
* **Descripción**: Compara la ruta óptima en distancia ($d$) contra la ruta óptima en tiempo ($t$) considerando los límites de velocidad (o la inferencia de velocidad según la clasificación de la calle `fclass`).
* **Justificación técnica**: Se realizan dos ejecuciones de Dijkstra:
  - Primera: Minimizando el peso de distancia (`weight` en metros).
  - Segunda: Minimizando el peso de tiempo (`time_weight` en segundos, calculado como $\text{distancia} / \text{velocidad}$).
  
  La comparación permite visibilizar el compromiso clásico de la planificación urbana: las autopistas o vías principales, a pesar de tener mayor longitud física, permiten velocidades significativamente mayores, resultando en menores tiempos de viaje totales.

---

## 3. Resultados Experimentales en Bolivia Dataset

A partir del procesamiento de los archivos `nodes.csv` (886,790 nodos) y `edges.csv` (588,485 aristas originales), se obtuvieron los siguientes resultados métricos:

### Estadísticas de Limpieza de Datos
* **Nodos Leídos**: 886,790 (100% válidos en límites de coordenadas de Bolivia).
* **Aristas Leídas**: 588,485
  - *Filtradas por no vehicular (peatonal/ciclovías)*: 62,294
  - *Filtradas por bucles propios (self-loops)*: 3,973
  - *Filtradas por duplicados*: 296
* **Aristas Válidas**: 521,922
* **Aristas Dirigidas en el Grafo (después de auto-reverse de calles de doble vía)**: **1,042,436**

### Resultados del Análisis de Grafo

#### A. Componentes Conexas e Islas Viales (Opción 2)
* **Cantidad total de islas (componentes)**: 375,890 (debido a la gran cantidad de nodos aislados o pequeños callejones desconectados de la red principal de transporte).
* **Componente Gigante**:
  - **Nodos**: 63,192
  - **Aristas dirigidas**: 143,564
  - **Grado promedio**: 2.2719
* **Grafo Completo Filtrado**:
  - **Nodos**: 886,790
  - **Aristas dirigidas**: 1,042,436
  - **Grado promedio**: 1.1755
* **Análisis**: El grado promedio aumenta significativamente en la componente gigante (2.27 vs 1.17), lo que demuestra que representa el núcleo de la red vial urbana interconectada de Bolivia, mientras que las islas menores consisten en segmentos aislados muy simples con baja conectividad.

#### B. Diámetro Vial (Opción 3)
* **Diámetro Estimado**: **2,114,583.86 metros** (aproximadamente **2,114.58 km**).
* **Nodo Origen**: `74204`
* **Nodo Destino**: `431256`
* **Tiempo de Cómputo**: **203 ms**.

#### C. Red de Emergencia Mínima - MST (Opción 4)
* **Distancia Total del MST**: **38,856.68 km** (longitud del árbol que interconecta todos los 63,192 nodos de la componente gigante sin ciclos).
* **Tiempo de Cómputo**: **217 ms**.

#### D. Caso de Estudio: Comparación de Rutas (Opción 5)
Ejecutando la búsqueda de rutas entre los dos extremos del diámetro vial (nodos `74204` y `431256`):

* **Ruta Óptima en Distancia**:
  - **Distancia**: 2,114,583.86 m (~2,114.58 km)
  - **Tiempo estimado**: 136,490.45 s (**37.91 horas**)
  - **Cantidad de calles (aristas)**: 1,511
* **Ruta Óptima en Tiempo**:
  - **Distancia**: 2,126,530.50 m (~2,126.53 km)
  - **Tiempo estimado**: 134,256.45 s (**37.29 horas**)
  - **Cantidad de calles (aristas)**: 1,675
* **Superposición**: Comparten **1,181 nodos** en común.
* **Tiempo de Cómputo**: **41 ms**.
* **Interpretación**: La ruta óptima en tiempo recorre 12 km más en distancia física, pero ahorra **37 minutos** de tiempo estimado al preferir vías de mayor velocidad jerárquica (autopistas/troncales).

---

## 4. Conclusiones

1. **Eficiencia de la Implementación**: Gracias al uso de estructuras eficientes nativas de C++ (vectores de adyacencia continuos en memoria, hashing plano para deduplicación y algoritmos iterativos con colas de prioridad), todos los análisis métricos sobre el mapa completo de Bolivia se resuelven en **menos de 250 milisegundos**.
2. **Estructura Vial de OpenStreetMap**: El alto número de componentes (375,890) refleja la realidad del dataset crudo de OSM, que contiene muchas zonas desconectadas, "islas" residenciales cerradas y vías rurales sin salida que no se enlazan formalmente con la red troncal nacional.
3. **Optimización con Heurísticas**: El uso de la heurística de doble barrido para el diámetro evitó una complejidad insuperable de $O(V^2 \log V)$, demostrando que en el análisis de redes viales a nivel de país, las aproximaciones fundamentadas son críticas para la viabilidad de la solución.
