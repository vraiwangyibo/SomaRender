我希望实现一个以 WebGPU 为核心的 C++ 跨平台的三维医疗数据渲染系统，计划这样的架构
层次,核心职责,技术选型
应用逻辑层 (App Layer),UI 交互、DICOM 加载调度、传输函数（TF）编辑。,C++20 / Dear ImGui
图形抽象层 (RHI Layer),将资源（纹理、Buffer）和管线（Pipeline）抽象。,webgpu.h (标准头文件)
后端驱动层 (Backend),真正把指令发给 GPU。,Dawn (Native) / WebGPU (Browser)

主要包含一下核心模块：
A. 3D 数据加载器 (Volume Loader)
B. 实时传输函数引擎 (Transfer Function Engine)
C. Ray Marching 渲染核 (The Shader)
