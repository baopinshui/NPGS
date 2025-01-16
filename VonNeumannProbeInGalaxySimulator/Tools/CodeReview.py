import re
import os
import sys
from enum import Enum
from typing import List, Dict, Tuple, Iterator, Optional, Match

class CodeReviewError:
    def __init__(self, line_no: int, message: str):
        self.line_no = line_no
        self.message = message

class NamingRuleChecker:
    def __init__(self):
        self.errors: List[CodeReviewError] = []
        
        # 增加新的正则表达式模式
        self.pascal_case_pattern = re.compile(r'^[A-Z][a-zA-Z0-9]*$')
        self.class_member_pattern = re.compile(r'^_[A-Z][a-zA-Z0-9]*$')
        self.const_pattern = re.compile(r'^k[A-Z][a-zA-Z0-9]*$')
        self.static_const_pattern = re.compile(r'^_k[A-Z][a-zA-Z0-9]*$')  # 新增
        self.bool_pattern = re.compile(r'^b[A-Z][a-zA-Z0-9]*$')
        self.global_bool_pattern = re.compile(r'^kb[A-Z][a-zA-Z0-9]*$')
        
    def check_pascal_case(self, name: str, line_no: int) -> bool:
        if not self.pascal_case_pattern.match(name):
            self.errors.append(CodeReviewError(line_no, f"命名 '{name}' 不符合帕斯卡命名法"))
            return False
        return True
        
    def check_variable_name(self, name: str, line_no: int, context: Dict):
        # 检查类成员变量
        if context.get('in_class'):
            if context.get('is_static') and context.get('is_const'):
                # 静态常量成员
                if not self.static_const_pattern.match(name):
                    self.errors.append(CodeReviewError(line_no, 
                        f"静态常量成员变量 '{name}' 应以_k开头并遵循帕斯卡命名法"))
                return
            elif context.get('is_static'):
                # 静态成员
                if not self.class_member_pattern.match(name):
                    self.errors.append(CodeReviewError(line_no, 
                        f"静态成员变量 '{name}' 应以_开头并遵循帕斯卡命名法"))
                return
            elif context.get('is_const'):
                # 常量成员
                if not self.static_const_pattern.match(name):
                    self.errors.append(CodeReviewError(line_no, 
                        f"常量成员变量 '{name}' 应以_k开头并遵循帕斯卡命名法"))
                return
            else:
                # 普通成员
                if not self.class_member_pattern.match(name):
                    self.errors.append(CodeReviewError(line_no, 
                        f"类成员变量 '{name}' 应以下划线开头并遵循帕斯卡命名法"))
                return

        # 检查常量
        if context.get('is_const'):
            if not self.const_pattern.match(name):
                self.errors.append(CodeReviewError(line_no, 
                    f"常量 '{name}' 应以k开头并遵循帕斯卡命名法"))
            return

        # 检查布尔变量
        if context.get('is_bool'):
            if context.get('is_global'):
                if not self.global_bool_pattern.match(name):
                    self.errors.append(CodeReviewError(line_no, 
                        f"全局布尔变量 '{name}' 应以kb开头并遵循帕斯卡命名法"))
            else:
                if not self.bool_pattern.match(name):
                    self.errors.append(CodeReviewError(line_no, 
                        f"布尔变量 '{name}' 应以b开头并遵循帕斯卡命名法"))
            return

        # 检查一般变量的帕斯卡命名法
        self.check_pascal_case(name, line_no)

    def check_class_name(self, name: str, line_no: int):
        if not self.pascal_case_pattern.match(name):
            self.errors.append(CodeReviewError(line_no, 
                f"类名 '{name}' 不符合帕斯卡命名法"))

    def check_enum_name(self, name: str, line_no: int):
        if not self.const_pattern.match(name):
            self.errors.append(CodeReviewError(line_no, 
                f"枚举值 '{name}' 应以k开头并遵循帕斯卡命名法"))

    def check_namespace_name(self, name: str, line_no: int):
        if not self.pascal_case_pattern.match(name):
            self.errors.append(CodeReviewError(line_no, 
                f"命名空间 '{name}' 不符合帕斯卡命名法"))

    def find_declarations(self, line: str, line_no: int):
        """分析代码行，找出声明并进行检查"""
        context = {
            'in_class': False,
            'is_const': False,
            'is_bool': False,
            'is_global': False,
            'is_static': False  # 新增静态标记
        }
        
        # 检测是否为静态变量
        context['is_static'] = 'static' in line
        
        # 类声明检查
        class_match = re.search(r'class\s+(\w+)', line)
        if class_match:
            self.check_class_name(class_match.group(1), line_no)
            
        # 变量声明检查
        var_matches = re.finditer(r'(?:const\s+)?(?:bool\s+)?(\w+)\s*[=;]', line)
        for match in var_matches:
            context['is_const'] = 'const' in line
            context['is_bool'] = 'bool' in line
            context['is_global'] = not any(c.isspace() for c in line[:match.start()])
            self.check_variable_name(match.group(1), line_no, context)

        # 命名空间检查
        namespace_match = re.search(r'namespace\s+(\w+)', line)
        if namespace_match:
            self.check_namespace_name(namespace_match.group(1), line_no)

        # 枚举检查
        enum_match = re.search(r'enum\s+(?:class\s+)?(\w+)', line)
        if enum_match:
            self.check_enum_name(enum_match.group(1), line_no)

class FormatRuleChecker:
    def __init__(self):
        self.errors: List[CodeReviewError] = []
        self.tab_size = 4  # 配置制表符等效空格数
        
    def check_line_length(self, line: str, line_no: int, max_length: int = 120):
        if len(line.rstrip()) > max_length:
            self.errors.append(CodeReviewError(line_no, f"行长度超过{max_length}个字符"))
    
    # def check_indentation(self, line: str, line_no: int):
    #     # 计算制表符等效的空格数
    #     expanded_line = line.expandtabs(self.tab_size)
    #     indent_count = len(expanded_line) - len(expanded_line.lstrip())
        
    #     if indent_count % 4 != 0:
    #         self.errors.append(CodeReviewError(line_no, 
    #             f"缩进必须是4个空格的倍数(当前缩进: {indent_count}空格)"))
            
    #     # 检查制表符与空格混用
    #     if '\t' in line and ' ' in line[:len(line) - len(line.lstrip())]:
    #         self.errors.append(CodeReviewError(line_no, 
    #             "不允许在同一行的缩进中混用制表符和空格"))

class CodeReviewer:
    def __init__(self):
        self.naming_checker = NamingRuleChecker()
        self.format_checker = FormatRuleChecker()
    
    @staticmethod
    def get_cpp_files(directory: str) -> Iterator[str]:
        """递归遍历目录获取所有C++源文件"""
        cpp_extensions = ('.cpp', '.h', '.hpp', '.inl')
        for root, _, files in os.walk(directory):
            for file in files:
                if file.lower().endswith(cpp_extensions):
                    yield os.path.join(root, file)
    
    def review_directory(self, directory: str) -> Dict[str, List[CodeReviewError]]:
        """检查指定目录下的所有C++文件"""
        results = {}
        for filepath in self.get_cpp_files(directory):
            try:
                errors = self.review_file(filepath)
                if errors:
                    results[filepath] = errors
            except Exception as e:
                print(f"检查文件 {filepath} 时发生错误: {str(e)}")
        return results

    def review_file(self, filepath: str) -> List[CodeReviewError]:
        if not os.path.exists(filepath):
            raise FileNotFoundError(f"找不到文件: {filepath}")
            
        with open(filepath, 'r', encoding='utf-8') as file:
            lines = file.readlines()
            
        for line_no, line in enumerate(lines, 1):
            # 检查格式规则
            self.format_checker.check_line_length(line, line_no)
            # self.format_checker.check_indentation(line, line_no)
            
            # 检查命名规则
            self.naming_checker.find_declarations(line, line_no)
        
        return self.naming_checker.errors + self.format_checker.errors

def main():
    if len(sys.argv) != 2:
        print("使用方法: python CodeReview.py <代码目录路径>")
        return

    directory = sys.argv[1]
    if not os.path.isdir(directory):
        print(f"错误: {directory} 不是有效的目录")
        return

    reviewer = CodeReviewer()
    try:
        results = reviewer.review_directory(directory)
        if results:
            print("\n发现以下问题:")
            for filepath, errors in results.items():
                print(f"\n文件: {filepath}")
                for error in errors:
                    print(f"  第 {error.line_no} 行: {error.message}")
        else:
            print("所有文件检查通过，未发现问题。")
    except Exception as e:
        print(f"执行检查时发生错误: {str(e)}")

if __name__ == "__main__":
    main()