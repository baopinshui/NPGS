import subprocess
import time
from pathlib import Path
from datetime import datetime

# 着色器类型及其文件扩展名
SHADER_TYPES = {
    '.comp.glsl': 'compute',
    '.frag.glsl': 'fragment',
    '.geom.glsl': 'geometry',
    '.mesh.glsl': 'mesh',
    '.rahit.glsl': 'ray any hit',
    '.rcall.glsl': 'ray callable',
    '.rchit.glsl': 'ray closest hit',
    '.rgen.glsl': 'ray generation',
    '.rint.glsl': 'ray intersection',
    '.rmiss.glsl': 'ray miss',
    '.task.glsl': 'task',
    '.tesc.glsl': 'tessellation control',
    '.tese.glsl': 'tessellation evaluation',
    '.vert.glsl': 'vertex',
}

# 配置
SOURCE_DIR = Path(__file__).parent.parent / 'Sources' / 'Engine' / 'Shaders'
TARGET_DIR = SOURCE_DIR.parent.parent.parent / 'Assets' / 'Shaders'
GLSLC_PATH = 'glslc.exe'

def parse_includes(shader_file: Path, included_files: set = None) -> set:
    """解析着色器文件中的所有 #include 指令"""
    if included_files is None:
        included_files = set()
    
    shader_dir = shader_file.parent
    try:
        with open(shader_file, 'r', encoding='utf-8') as f:
            for line in f:
                if line.strip().startswith('#include'):
                    # 提取包含文件的路径
                    include_path = line.split('"')[1]
                    full_path = (shader_dir / include_path).resolve()
                    
                    if full_path not in included_files:
                        included_files.add(full_path)
                        # 递归解析包含的文件
                        parse_includes(full_path, included_files)
    except Exception as e:
        print(f"警告: 解析包含文件时出错 {shader_file.name}: {str(e)}")
    
    return included_files

def needs_recompile(source_file: Path, spv_file: Path) -> bool:
    """检查源文件及其依赖是否需要重新编译"""
    if not spv_file.exists():
        return True
    
    # 获取目标文件的修改时间
    target_time = spv_file.stat().st_mtime
    
    # 检查主文件
    if source_file.stat().st_mtime > target_time:
        return True
    
    # 检查所有包含的文件
    try:
        included_files = parse_includes(source_file)
        for include_file in included_files:
            if include_file.stat().st_mtime > target_time:
                return True
    except Exception as e:
        print(f"警告: 检查依赖文件时出错 {source_file.name}: {str(e)}")
        return True  # 如果出错，为安全起见返回True
    
    return False

def compile_shader(source_file: Path, target_file: Path) -> bool:
    """编译着色器文件"""
    target_file.parent.mkdir(parents=True, exist_ok=True)
    
    try:
        result = subprocess.run(
            [GLSLC_PATH, '-O', str(source_file), '-o', str(target_file)],
            capture_output=True,
            text=True
        )
        
        if result.returncode != 0:
            # 获取相对路径
            rel_path = source_file.relative_to(SOURCE_DIR)
            print(f"{result.stderr}")
            return False

        print(f"{target_file.name}")
        return True
    except Exception as e:
        rel_path = source_file.relative_to(SOURCE_DIR)
        print(f"{rel_path}: error: {str(e)}")
        return False

def main() -> tuple:
    """主函数"""
    current_time = datetime.now().strftime("%H:%M")
    print(f"生成开始于 {current_time}...")
    print("------ 正在编译着色器 ------")
    
    compiled_count = 0
    skipped_count = 0
    failed_count = 0
    
    TARGET_DIR.mkdir(parents=True, exist_ok=True)
    
    for shader_ext in SHADER_TYPES.keys():
        for source_file in SOURCE_DIR.rglob(f"*{shader_ext}"):
            rel_path = source_file.relative_to(SOURCE_DIR)
            base_name = source_file.name.replace('.glsl', '')
            target_file = TARGET_DIR / rel_path.parent / f"{base_name}.spv"
            
            if needs_recompile(source_file, target_file):
                print(f"{source_file.name}")
                if compile_shader(source_file, target_file):
                    compiled_count += 1
                else:
                    failed_count += 1
            else:
                skipped_count += 1

    return compiled_count, failed_count, skipped_count

if __name__ == "__main__":
    start_time = time.time()
    compiled, failed, skipped = main()
    elapsed_time = time.time() - start_time
    
    print(f"========== 生成: {compiled} 成功，{failed} 失败，{skipped} 最新，0 已跳过 ==========")
    print(f"========== 生成 于 {datetime.now().strftime('%H:%M')} 完成，耗时 {elapsed_time:.3f} 秒 ==========")
