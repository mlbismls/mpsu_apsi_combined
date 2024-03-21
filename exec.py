import subprocess

# 定义要执行的命令
commands_1 = ["./main -r 0", "./main -r 1", "./main -r 2"]
commands_2 = ["./receiver_cli -q /app/APSI/build/query.csv -t 1 -o /app/APSI/build/test.csv","./sender_cli -d /app/APSI/build/out.csv -p /app/APSI/parameters/1M-256.json -c -t 1 "]
commands_3 = "cd /app/MPSU/build/ && "
commands_4 = "cd /app/APSI/build/bin/ && "

# 创建子进程执行命令
processes_1 = []
for command in commands_1:
    process = subprocess.Popen(commands_3 + command, shell=True)
    processes_1.append(process)

# 等待第一组命令执行完成
for process in processes_1:
    process.wait()

# 创建子进程执行第二组命令
processes_2 = []
for command in commands_2:
    process = subprocess.Popen(commands_4 + command, shell=True)
    processes_2.append(process)

# 等待第二组命令执行完成
for process in processes_2:
    process.wait()

print("All commands have been executed successfully.")

