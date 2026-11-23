# Player 项目重开验收

状态为当前代码的实际 Player 验证通过。
代码版本为 `2b2b007`，覆盖 Debug 和 Release。
两设备分别为 NVIDIA RTX 4060 Laptop GPU 和 Intel UHD。
本项复用集中回归的匹配程序、输入及环境。

## 输入与结果

输入在第 4 帧按 F，第 5 帧释放。
开关改变导光板位置、旋转及检修灯位置。
每次启动均检查初始关闭状态和最终启用状态。
第 4 帧起的反射满足解析镜面区域的信号门槛。

十二次启动包含首次、同内容重开及同路径重开。
四种配置与设备组合的十二帧均精确一致。
同路径复验直接读取首次运行的项目及动作文件。
输入事件数、脚本诊断、呈现及退出释放均通过。

## 项目入口

[集中验证归档](../final-original-reflection/validation.zip)包含可启动项目。
解压后使用 `release-nvidia-first/project.azureproject`。
同一项目也可由对应 Debug 程序加载。
入口参数为 `--project`、`--qa-camera scene` 和 `--qa-light scene`。

自动复现按共享队列运行，输出指向受管任务目录。
`actions.json` 提供已冻结的真实键位输入回放。
普通交互窗口的实体按键及主观画面观察为待人工复验。
该复验仅在用户主动点名后安排。

## 证据复算

解压上述归档并运行 `python verify.py`。
该入口复算完整图像、输入状态及 GPU 附件。
[结构化收据](qualification.json)绑定集中归档摘要及程序身份。
