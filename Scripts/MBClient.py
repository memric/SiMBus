from pymodbus.client.sync import ModbusSerialClient as ModbusClient
class MBClient:

    def __init__(self, dev, addr, baudrate=9600, timeout=1):
        self.dev = dev
        self.addr = addr
        self.baudrate = baudrate
        self.timeout = timeout
        self.client = ModbusClient(method='rtu', port=self.dev, timeout=self.timeout, baudrate=self.baudrate)
        self.connect()
        
    #Connects to controller as ModBus client
    def connect(self):
        self.client.connect()
                
    #reads registers
    def reg_read(self, start_addr, count):
        rr = self.client.read_holding_registers(start_addr, count, unit=self.addr)
        if (not rr.isError()):
            return rr.registers
        else:
            return None

    #writes registers
    def reg_write(self, start_addr, value):
        if isinstance(value, list):
            rq = self.client.write_registers(start_addr, value, unit =self.addr)
            if (not rq.isError()):
                return len(value)
            else:
                return 0
        else:
            rq = self.client.write_register(start_addr, value, unit=self.addr)
            if (not rq.isError()):
                return 1
            else:
                return 0
