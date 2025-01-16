import os
import numpy as np
import matplotlib.pyplot as plt
from scipy import integrate

# 参数
start = np.log10(0.075)
end = np.log10(300)
step = 0.04

# 单星分布
def pdf(lgx):
    return np.where(10**lgx <= 1,
                    0.158 * np.exp(-(lgx + 1)**2 / 1.101128),
                    0.06371598 * (10**lgx)**-0.8)

# 双星分布
def pdf2(logx):
    return np.where(10**logx <= 1,
                    0.086 * np.exp(-(logx + 0.65757734)**2 / 1.101128),
                    0.058070157 * (10**logx)**-0.65)

# 概率密度函数的积分
total_area, _ = integrate.quad(pdf2, start, end)

def read_data(filename):
    with open(filename, 'r') as f:
        data = f.read().split(',')
    return [float(x) for x in data if x.strip()]

def create_bins(start, end, step):
    return np.arange(start, end + step, step)

def classify_data(data, bins):
    counts, _ = np.histogram(data, bins=bins)
    return counts

def plot_histogram(bins, counts, total_samples):
    plt.figure(figsize=(12, 6))
    
    # 画出统计图，顺便缩放
    scaled_counts = counts / (total_samples * step / total_area)
    plt.bar(bins[:-1], scaled_counts, width=step, align='edge', alpha=0.7)
    
    # 画出理论概率密度曲线
    x = np.linspace(start, end, 1000)
    plt.plot(x, pdf2(x), 'r-', linewidth=2, label='PDF')
    
    plt.xlabel('Log10(Value)')
    plt.ylabel('Scaled Count / Probability Density')
    plt.title('Distribution of Generated Data (Log Scale)')
    plt.grid(True, alpha=0.3)
    
    # x轴坐标
    log_ticks = np.arange(np.ceil(start), np.floor(end) + 1, 0.5)
    plt.xticks(log_ticks, [f'{x:.1f}' for x in log_ticks])
    
    plt.legend()
    plt.show()

if __name__ == "__main__":
    # 获取当前脚本的目录
    script_dir = os.path.dirname(os.path.abspath(__file__))
    # 构建相对路径
    filename = os.path.join(script_dir, "BinarySecondStar.csv")
    
    data = read_data(filename)
    
    # 取对数，过滤掉不在区间内的数据
    log_data = [np.log10(x) for x in data if x > 0]
    filtered_data = [x for x in log_data if start <= x <= end]
    
    bins = create_bins(start, end, step)
    counts = classify_data(filtered_data, bins)
    
    plot_histogram(bins, counts, len(filtered_data))