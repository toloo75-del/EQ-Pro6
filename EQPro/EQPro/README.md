# EQ Pro

Ecualizador paramétrico de 8 bandas en formato **VST3** para Windows (hecho con JUCE).

## Características
- 8 bandas: Bell, Low Shelf, High Shelf, Low Cut, High Cut, Notch, Band Pass
- Frecuencia 20 Hz – 20 kHz, ganancia ±24 dB, Q 0.1 – 18
- Curva de respuesta en tiempo real y analizador de espectro
- Edición directa en la gráfica:
  - **Doble clic** en vacío: añade una banda
  - **Doble clic** sobre un nodo: la quita
  - **Arrastrar** nodo: frecuencia y ganancia
  - **Rueda del ratón**: Q
- Ganancia de salida, guardado de estado en el proyecto

## Compilar en Windows (opción fácil: GitHub Actions)
1. Crea un repositorio en GitHub y sube esta carpeta.
2. Ve a la pestaña **Actions** → "Build EQ Pro" → espera a que termine.
3. Descarga el artefacto **EQ-Pro-VST3-Windows** (contiene `EQ Pro.vst3`).
4. Copia `EQ Pro.vst3` a `C:\Program Files\Common Files\VST3\`
5. Reescanea plugins en tu DAW.

## Compilar en local
Requisitos: Visual Studio 2022 (carga de trabajo "Desarrollo para el escritorio con C++") y CMake.

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target EQPro_VST3
```
El resultado queda en `build/EQPro_artefacts/Release/VST3/EQ Pro.vst3`.
(También se genera una versión Standalone con el target `EQPro_Standalone`.)

## Licencia
JUCE se descarga automáticamente (licencia AGPLv3 / comercial). Si distribuyes el plugin
públicamente, revisa los términos de licencia de JUCE.
