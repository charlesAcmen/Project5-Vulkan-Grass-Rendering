# Project 5：Vulkan Grass Rendering 路线图

这是项目的推荐实现顺序。它遵循作业说明的建议：先验证静态草叶的完整
渲染路径，再逐层加入模拟和 GPU visibility。

## 数据流总览

```text
CPU 生成 Blade 数据
    -> source Blade buffer
    ->（阶段 2）Compute physics simulation
    ->（阶段 3）visible Blade buffer + indirect draw command
    -> vertex / tessellation / fragment shaders
    -> swapchain image
```

`Blade` 是 CPU 与 GPU 之间的 ABI：包含 `v0`、`v1`、`v2`、`up` 四个
`vec4`。它们的 `.w` 依次存储 orientation、height、width、stiffness。
修改时必须保持 C++ 与 GLSL 的字段顺序完全一致。

## 阶段 0：环境与可验证基线

### 目标

在改动渲染逻辑前，先让 starter scene 能以启用 Validation Layers 的 Debug
配置构建并运行。

### 要做的事

- 安装 Windows Vulkan SDK，其中应包含 Validation Layers 和
  `glslangValidator`。
- 确认 `VULKAN_SDK` 指向 SDK 根目录。工程的 CMake 会通过这个标准变量寻找
  shader compiler。
- 从 x64 Visual Studio Developer Shell 配置 Debug 构建：

  ```powershell
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
  cmake --build build
  ```

- 从 `bin/` 运行程序。CMake 将 SPIR-V 放在 `bin/shaders/`，将运行时图片放在
  `bin/images/`。
- “能构建”只说明 build 通过；Vulkan 资源是否正确以 Validation Layers 和实际
  窗口画面为准。

### 完成标准

地面平面可显示，且没有 Vulkan validation error。

## 阶段 1：静态草地垂直切片（已落地）

### 目标

将每根草渲染为 tessellated Bezier ribbon。此阶段没有风、物理更新、culling、
compaction 或 indirect draw。

### 已实现的点

- `Blades` 显式拥有三个 GPU buffer：source blades、未来的 visible blades、
  以及未来的 indirect draw arguments。
- source buffer 同时带有 vertex-buffer 与 storage-buffer usage，为后续
  Compute 阶段保留同一份 ABI。
- grass 使用独立 descriptor set，只绑定 group model matrix，不再错误复用
  带 terrain texture 的 model descriptor。
- `grass.vert` 转发完整的 `Blade` record。
- `grass.tesc` 采用一个 control point 对应一根草，并给中心线设置固定细分。
- `grass.tese` 计算二次 Bezier centerline，再利用 orientation、width、up
  扩展为逐渐收尖、双面的 ribbon。
- `grass.frag` 使用随高度变化的绿色与稳定的方向光。
- Renderer 在绘制地面后，绑定 source Blade VBO，直接绘制全部 `NUM_BLADES`。

### 阅读入口

- `src/Blades.h`、`src/Blades.cpp`：Blade ABI 与 GPU 资源。
- `src/Renderer.cpp`：descriptor、grass pipeline、command buffer。
- `src/shaders/grass.*`：四级图形管线中的数据传递。

### 完成标准

画面中出现密集的静态草场；旋转、缩放相机时草叶的宽度、曲线和高度渐变稳定；
validation 不报告 descriptor、vertex input 或 tessellation 错误。

## 阶段 2：Compute 物理模拟

### 目标

每帧更新草叶 `v2` physical guide，并修正 `v1/v2`，以保持草叶长度且避免 tip
穿过地面。

### 要做的事

- 为 source blade storage 与 simulation inputs 新建 Compute descriptor set。
- 每帧 dispatch `ceil(NUM_BLADES / WORKGROUP_SIZE)` 个 invocation。
- 加入 environmental gravity、front gravity、Hooke-law recovery 与随时间变化的
  procedural wind。
- 按论文第 5.2 节修正 `v1/v2`，不能只对 tip 直接累加 force translation。
- `v0` 根部始终不能移动。

### 完成标准

风可平滑带动草叶，根部保持固定；关闭风后草叶会向初始直立状态恢复。

## 阶段 3：GPU visibility 与 indirect draw

### 目标

将可见草叶 compact 到 GPU buffer，并只通过 `vkCmdDrawIndirect` 绘制该 buffer。

### 要做的事

- 以 blade facing direction 和 view vector 做 orientation culling。
- 对 `v0`、`v2`、加权 Bezier midpoint 做 frustum test。
- 用 distance buckets 逐级丢弃远处草叶。
- 用 atomic counter 分配 visible buffer 输出槽位，并将计数写入 indirect draw command
  的 `vertexCount`。
- 用 visible VBO + indirect command 替换阶段 1 的 direct draw。
- 通过 compute-finished semaphore 与 buffer barrier 处理 compute 到 vertex-input /
  indirect-command read 的同步；只有 graphics/compute queue family 不同时才增加
  ownership transfer。

### 完成标准

移动相机时渲染草叶数量会变化但无闪烁；视锥外草叶不再绘制；validation 不报告
synchronization error。

## 阶段 4：性能证据与展示

### 目标

完成作业要求的性能分析，并逐步完善 README。

### 要做的事

- 固定场景和相机，在不同 blade count 下记录 frame time。
- 先记录 baseline，再分别开启 orientation、frustum、distance culling，最后测量
  三者组合。
- 每组数据都记录 GPU/CPU timing 方法、分辨率、build type、driver、hardware。
- 分别在静态草、物理模拟、culling 确认可用后录制 GIF。

### 完成标准

README 中的性能结论都有可复现的测量支撑，而不是仅依据 build output。

## 阶段 5：可选扩展

- 随距离变化的 tessellation LOD。
- ImGui 风场控制或推动草叶的移动 collider。
- 加入场景物体和 depth map 后的 occlusion culling。
- Skybox、颜色变化或更复杂的草叶光照。

扩展功能应与基础路径隔离；即使实验性功能回归，基本项目仍可独立演示。
