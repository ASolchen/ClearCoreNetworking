from pymodbus.client import ModbusTcpClient


with ModbusTcpClient('192.168.10.177') as clear_core:
    res = clear_core.read_coils(1,10)


print(res)