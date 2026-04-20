# NWSpeech

基于 C++ 和 Qt 6 的 Windows 实时语音识别应用。

NWSpeech 使用 [sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx) 作为语音识别引擎，支持捕获系统音频或麦克风输入进行实时识别，并提供浮窗字幕功能。

## 功能特性

- **实时音频捕获** — 基于 Windows Core Audio API，支持三种模式：系统音频（loopback）、麦克风、或同时捕获
- **双管线独立识别** — 系统音频和麦克风各自独立运行 VAD + ASR 管线，结果带来源标签（`[系统]` / `[麦克风]`）
- **多种 ASR 模型** — Streaming Zipformer（快速）、FireRed ASR CTC（均衡）、FireRed ASR（最佳精度）
- **语音活动检测** — 基于 Silero VAD 自动检测语音片段
- **浮窗字幕** — 始终置顶、可拖拽、可自定义外观
- **文件保存** — 识别结果（TXT，带时间戳）和音频（WAV，增量写入）
- **离线识别** — 无需实时捕获，直接转录 WAV 文件
- **系统托盘** — 最小化到托盘，后台持续识别
- **LLM 文本优化** — 支持通过大语言模型修正错词、添加标点、断句，可配置 API 地址、模型名等

## 项目结构

```
NWSpeech/
├── CMakeLists.txt
├── README.md
├── resources.qrc
├── app.rc
├── src/
│   ├── main.cpp
│   ├── config.h / config.cpp
│   ├── core/
│   │   ├── audio_capture.h / audio_capture.cpp
│   │   ├── audio_resampler.h / audio_resampler.cpp
│   │   ├── voice_activity_detector.h / voice_activity_detector.cpp
│   │   ├── speech_recognizer.h / speech_recognizer.cpp
│   │   └── file_saver.h / file_saver.cpp
│   ├── service/
│   │   ├── speech_recognition_service.h / speech_recognition_service.cpp
│   │   ├── offline_recognition_service.h / offline_recognition_service.cpp
│   │   └── llm_optimizer.h / llm_optimizer.cpp
│   └── ui/
│       ├── main_window.h / main_window.cpp
│       ├── settings_dialog.h / settings_dialog.cpp
│       └── floating_window.h / floating_window.cpp
├── 3rd/
│   └── sherpa-onnx/
│       ├── include/
│       └── lib/
└── models/
    ├── silero_vad.onnx
    ├── fire-red-asr2-ctc/
    ├── fire-red-asr2/
    └── streaming-zipformer/
```

## 架构设计

```
┌─────────────────────────────────────────────────┐
│                    UI 层                         │
│  MainWindow · SettingsDialog · FloatingWindow    │
├─────────────────────────────────────────────────┤
│                  服务层                           │
│  SpeechRecognitionService · OfflineRecService    │
├─────────────────────────────────────────────────┤
│                  核心层                           │
│  AudioCapture · AudioResampler · VAD · ASR       │
│                  FileSaver                       │
├─────────────────────────────────────────────────┤
│                  配置层                           │
│            AppConfig · ModelConfig               │
└─────────────────────────────────────────────────┘
```

- **UI 层** — Qt 6 界面组件，负责用户交互和显示
- **服务层** — 业务逻辑编排核心组件，通过信号槽与 UI 层通信
- **核心层** — 独立的功能模块：音频捕获、重采样、VAD、识别、文件 I/O
- **配置层** — 应用配置管理，支持 INI 持久化

## 环境要求

- Windows 10 或更高版本
- Visual Studio 2022（MSVC）
- CMake 3.16+
- Qt 6（MSVC 2022 64-bit）
- sherpa-onnx（已包含在 `3rd/` 目录）

## 构建

```bash
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 --fresh

cmake --build build --config Release
```

*注：请确保已设置环境变量 `QtDir` 指向 Qt 安装目录（如 `C:/Qt/6.11.0/msvc2022_64`），或在 CMake 命令中通过 `-DCMAKE_PREFIX_PATH` 指定 Qt 路径。*

运行：

```bash
build\Release\nwspeech.exe
```

## 使用说明

1. **启动程序** — 运行 `nwspeech.exe`
2. **配置设置** — 点击 `更多 → 设置`，配置音频来源、模型、输出目录等
3. **LLM 优化配置** — 在设置的 `LLM 优化` 标签页中，可配置：
   - 启用 LLM 优化（默认关闭）
   - API 地址（默认 `http://127.0.0.1:1234/v1`）
   - 模型名称（默认 `gemma-4-e2b-it`）
   - API Key（默认 `default_key`）
   - 上下文句子数（默认 3）
4. **开始识别** — 点击 `开始` 按钮
5. **查看结果** — 实时结果显示在上方，最终结果带时间戳记录在日志区
6. **浮窗字幕** — 点击 `浮窗` 切换始终置顶的字幕窗口
7. **离线识别** — 点击 `更多 → 离线识别` 转录 WAV 文件
8. **停止识别** — 点击 `停止` 按钮

## 模型说明

将模型文件放在可执行文件同级的 `models/` 目录下：

| 模型 | 目录 | 所需文件 |
|------|------|----------|
| Silero VAD | `models/` | `silero_vad.onnx` |
| Streaming Zipformer | `models/streaming-zipformer/` | `encoder.onnx`、`decoder.onnx`、`joiner.onnx`、`tokens.txt` |
| FireRed ASR CTC | `models/fire-red-asr2-ctc/` | `model.onnx`、`tokens.txt` |
| FireRed ASR | `models/fire-red-asr2/` | `encoder.onnx`、`decoder.onnx`、`tokens.txt` |

## 依赖库

| 库 | 用途 |
|----|------|
| Qt 6 (Core, Widgets, Gui) | GUI 框架 |
| sherpa-onnx | 语音识别引擎 |
| onnxruntime | ONNX 模型推理 |
| Windows Core Audio | 音频捕获（COM API） |

## 扩展开发

### 添加新模型类型

1. 在 `config.h` 的 `ModelType` 枚举中添加新值
2. 在 `core/speech_recognizer.cpp` 的 `SpeechRecognizer::Initialize()` 中添加初始化逻辑
3. 在 `config.cpp` 的 `InitializeModelConfig()` 中添加模型路径映射
4. 在 `ui/settings_dialog.cpp` 中添加下拉选项

### 修改音频处理

- 捕获逻辑：`core/audio_capture.cpp`
- 重采样逻辑：`core/audio_resampler.cpp`

### 修改文件输出

- 文本/音频保存：`core/file_saver.cpp`

## 许可证

本项目使用 sherpa-onnx（Apache 2.0）和 Qt 6（LGPLv3），请参考各自的许可证。
