import os
import numpy as np
import matplotlib.pyplot as plt

# Get the data from the Results folder
path = 'Results/ECN'

# q1Size.csv and q2Size.csv
q1Size = np.genfromtxt(os.path.join(path, 'q1Size_ECN.csv'), delimiter=',')
q2Size = np.genfromtxt(os.path.join(path, 'q2Size_ECN.csv'), delimiter=',')

plt.figure(figsize=(10, 5))
plt.plot(q1Size[:, 0], q1Size[:, 1], label='Queue 1 Size', color='blue', linewidth=1.5)
plt.plot(q2Size[:, 0], q2Size[:, 1], label='Queue 2 Size', color='green', linewidth=1.5)
plt.xlabel('Time')
plt.ylabel('Queue Size')
plt.title('Queue Sizes Over Time')
plt.legend()
plt.grid()
plt.savefig(path + 'queue_sizes.png')
plt.close()  # Close the figure after saving to avoid overlap

# throughput.csv
throughput = np.genfromtxt(os.path.join(path, 'throughput_ECN.csv'), delimiter=',', dtype=str)
flows = {
    "Flow 1": {"src_ip": "192.168.1.2", "data": {}, "color": "blue", "label": "Main Flow 1"},
    "Flow 2": {"src_ip": "192.168.2.2", "data": {}, "color": "orange", "label": "Main Flow 2"},
    "Flow 3": {"src_ip": "192.168.5.2", "dst_ip": "192.168.6.2", "data": {}, "color": "purple", "label": "Background Flow 1"},
    "Flow 4": {"src_ip": "192.168.7.2", "dst_ip": "192.168.8.2", "data": {}, "color": "red", "label": "Background Flow 2"},
    "Flow 5": {"src_ip": "192.168.5.2", "dst_ip": "192.168.7.2", "data": {}, "color": "brown", "label": "Background Flow 3"},
    "Flow 6": {"src_ip": "192.168.5.2", "dst_ip": "192.168.8.2", "data": {}, "color": "gray", "label": "Background Flow 4"},
    "Flow 7": {"src_ip": "192.168.6.2", "dst_ip": "192.168.7.2", "data": {}, "color": "pink", "label": "Background Flow 5"},
    "Flow 8": {"src_ip": "192.168.6.2", "dst_ip": "192.168.8.2", "data": {}, "color": "cyan", "label": "Background Flow 6"},
}

# Organize throughput data per flow
# Skip the first row
for row in throughput[1:]:
    time, src_ip, dst_ip, throughput_value = float(row[0]), row[1], row[3], float(row[5])
    for flow_name, flow_info in flows.items():
        if flow_info["src_ip"] == src_ip and flow_info.get("dst_ip", dst_ip) == dst_ip:
            flow_info["data"][time] = throughput_value

# Plotting throughput for each flow with distinct styles
plt.figure(figsize=(12, 8))
for flow_name, flow_info in flows.items():
    times = list(flow_info["data"].keys())
    values = list(flow_info["data"].values())
    if flow_name in ["Flow 1", "Flow 2"]:
        plt.plot(times, values, label=flow_info["label"], color=flow_info["color"], linewidth=2, alpha=1.0)
    else:
        plt.plot(times, values, label=flow_info["label"], color=flow_info["color"], linewidth=1, alpha=0.8)

plt.xlabel('Time')
plt.ylabel('Throughput')
plt.title('Throughput per Flow Over Time')
plt.legend()
plt.grid()
plt.savefig(path + 'throughput.png')
plt.close()  # Close the figure after saving to avoid overlap
