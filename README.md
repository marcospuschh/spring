# PTZR Spring

Spring reverb (VST3 / AU) da marca **PTZR.PLUGINS**. Mono→estéreo e estéreo→estéreo.

Controles: Mix, Decay (0.5–10 s), Tone, Pre-Delay (0–200 ms), Drive (fita), Damping, Low Cut,
Spring Type (Vintage / Classic / Modern) e 5 presets de fábrica (+ INIT). O botão SAVE grava presets do usuário em
`Documentos/PTZR.PLUGINS/PTZR Spring/Presets`.

## Build local
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

## Build pelo GitHub
Suba o repositório → aba **Actions** → **Build PTZR Spring** → baixe o artefato (macOS / Windows).

## Instalar no macOS
Descompacte `PTZR-Spring-macOS.zip` e rode `bash install_mac.command`.
VST3 vai para `~/Library/Audio/Plug-Ins/VST3/PTZR.PLUGINS/` e o AU para `~/Library/Audio/Plug-Ins/Components/`.

## Instalar no Windows
Copie a pasta `PTZR.PLUGINS` (de dentro de `VST3`) para `C:\Program Files\Common Files\VST3\`.
