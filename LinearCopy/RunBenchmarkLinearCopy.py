import subprocess
import os
import time
import matplotlib.pyplot as plt 
def run_simple_test(tryCount = 8):
    """Simple version that just runs all sizes"""
    
    program = "..\\x64\\Release\\GpuCopy.exe"
    
    if not os.path.exists(program):
        print(f"Error: {program} not found!")
        return
  
    csv_file = "bandwidth_results.csv"
    if os.path.exists(csv_file):
        os.remove(csv_file)        


    for size in range(8, 1024, 128):
        print(f"\nRunning size {size}...")
        
        # Run 8 times for each size
        for i in range(tryCount):
            time.sleep(0.01)
            try:
                result = subprocess.run([
                    program,
                    str(size),
                    str(size),
                    str(size),
                    str(size),
                    str(0)
                ], capture_output=True, text=True)
                print(f"  Run {i+1}: {result.stdout.strip()}")
            except Exception as e:
                print(f"  Run {i+1}: Error - {e}")

def plot_bandwidth_results(filename):
    #read from navi48_bandwidth_resutls.
    size = []
    bandwidth = []
    with open(filename, "r") as f:
        # skip first line
        f.readline()
        lines = f.readlines()
        for line in lines:
            parts = line.strip().split(',')
            size.append(int(parts[0]))
            bandwidth.append(float(parts[4]))
        
    print(size)
    print(bandwidth)
    # plot using matplotlib
    plt.plot(size, bandwidth, marker='o')
    plt.xlabel('Size')
    plt.ylabel('Bandwidth (GB/s)')
    plt.title('LinearCopy Bandwidth Results')
    plt.grid(True)
    plt.savefig('LinearCopyBandwidth.pdf')   # PDF format
    plt.show() 

if __name__ == "__main__":
    run_simple_test()
    plot_bandwidth_results("bandwidth_results.csv")
