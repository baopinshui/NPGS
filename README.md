大概是世界上最好的最大延拓kn时空渲染着色器，在NPGS/Sources/Engine/Shaders/BlackHole_common.glsl
（下面的介绍是我直接从自己的shadertoy搬过来的）
本Shader实现了基于物理的克尔-纽曼时空（带电旋转黑洞）光线追踪。
This shader implements physically based ray-tracing in Kerr-Newman spacetime(charged rotating black hole).

实现了完整的时空拓扑：外视界、内视界、内外能层、奇环，以及反宇宙。
Implements complete spacetime topology: outer/inner event horizons,
outer/inner ergospheres, ringularity, and the antiverse.

实现了完整的克尔-纽曼时空最大延拓解。光线可以穿过内视界进入
全新的时空域（白洞区域），并最终抵达另一个宇宙。
Implements the complete maximal extension of Kerr-Newman spacetime. 
Rays can traverse the inner horizon into new spacetime domains (white hole regions) 
and eventually emerge into another  universe.

实现了基于物理的偏振（极化）模拟。通过在弯曲时空中计算
Walker-Penrose常数，精确模拟吸积盘发光在强引力场下的偏振态演化。
Implements physically-based polarization simulation. By calculating 
Walker-Penrose constants in curved spacetime, it accurately simulates the 
evolution of polarization states from the accretion disk under strong gravity.

支持裸奇点。支持切换静态观者、自由落体观者以及自定义坐标速度观者。
Supports naked singularities. Supports Static, Free-falling, and Custom-velocity observers.

完全拟真广义相对论效应，包含吸积盘、相对论喷流、引力透镜、
多普勒频移、引力红移以及热浪折射。
Fully simulates GR effects: accretion disk, relativistic jets, gravitational
lensing, Doppler shift, gravitational redshift, and heat haze refraction.


完整可运行的在x64里那个压缩包



操作：首先shift关输入法（笑
然后，绕转模式下wasd是横向和纵向环绕，鼠标拖动也是。
此外，绕转模式下鼠标右键拖动是偏头，中键回正（只在绕转模式）
自由模式下ws前后   ad左右  rf上下   qe滚转   鼠标转视角 T切换模式 G使得相机沿测地线移动 H隐藏ui o……（）
第三个左侧按钮可以输指令，tp，格式是/tp x y z t ux uy uz
