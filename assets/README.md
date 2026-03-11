# 体数据资源

将 Raw 体数据文件放在此目录，或通过命令行指定路径。

## 格式

- **无头 Raw**：纯二进制，约定尺寸。运行示例：
  ```bash
  ./SomaRender volume.raw 256 256 256
  ```
  默认 8-bit；16-bit 需在代码中指定。

- **带头 Raw**：前 16 字节为 `width(4) height(4) depth(4) bits(4)`，随后为体数据。需在代码中调用 `loadRawWithHeader(path)`。

## 示例数据

可使用任意 256³ 或 128³ 的 8-bit Raw 文件测试；无示例文件时程序会报错并退出，请自备或生成测试数据。
