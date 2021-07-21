#!/usr/bin/python
# encoding: utf-8

import sys
import csv
import datetime

REG_MIN_VALUE = 0
REG_MAX_VALUE = 65535

def main(argv=None): # IGNORE:C0111
	with open('test.csv', newline='') as csvfile:
		reader = csv.DictReader(csvfile)
		last_reg_addr = 0
		reg_map = []
		
		print("Check input file")
		
		'''Check CSV table content'''
		try:
			for row in reader:
				addr = int(row['Address'])
				if addr < REG_MIN_VALUE or addr > REG_MAX_VALUE:
					print("Error: Address of register \"%s\" is not correct"%(row['Name']))
					sys.exit(-1)
					
				min = int(row['Min'])
				if min < REG_MIN_VALUE or min > REG_MAX_VALUE:
					print("Error: Minimum value of register \"%s\" is not correct"%(row['Name']))
					sys.exit(-1)
					
				max = int(row['Max'])
				if max < REG_MIN_VALUE or max > REG_MAX_VALUE:
					print("Error: Maximum value of register \"%s\" is not correct"%(row['Name']))
					sys.exit(-1)
					
				if min >= max:
					print("Warning: Minimum value of register \"%s\" is equal or greiter than maximum value"%(row['Name']))
					
				default = int(row['Default'])
				if default < REG_MIN_VALUE or default > REG_MAX_VALUE:
					print("Error: Default value of register \"%s\" is not correct"%(row['Name']))
					sys.exit(-1)
					
				print("Register: \"%s\"; Addr: %s; Min: %d; Max: %d; Default: %d"%(row['Name'], hex(addr), min, max, default))
				
				last_reg_addr = addr
				reg_map.append(row)
		except csv.Error as e:
			sys.exit('file {}, line {}: {}'.format(filename, reader.line_num, e))
			
		print("Last Address: %s"%(hex(last_reg_addr)))
		reg_num = last_reg_addr + 1
		print("Registers number: %s"%(reg_num))
		
	'''Create C files'''
	'''Register map'''
	rmh_f = open('reg_map.h', 'w', newline='')
	#Comment
	rmh_f.write("/**\r\n* This file is created automatically\r\n")
	rmh_f.write("* Created on: %s\r\n**/\r\n"%(datetime.date.today()))
	rmh_f.write("\r\n")
		
	#Defines
	rmh_f.write("#ifndef REG_MAP_H_\r\n")
	rmh_f.write("#define REG_MAP_H_\r\n")
	rmh_f.write("\r\n")
		
	for row in reg_map:
		addr = int(row['Address'])
		min = int(row['Min'])
		max = int(row['Max'])
		default = int(row['Default'])
		oper = 0
		if row['Mode'].upper() == 'R':
			oper = 1
		elif row['Mode'].upper() == 'W':
			oper = 2
		elif row['Mode'].upper() == 'RW':
			oper = 3
			
		rmh_f.write("/* Register: %s\r\n* Addr: %s; Min: %d; Max: %d; Default: %d\r\n*/\r\n"%(row['Comment'], hex(addr), min, max, default))
		rmh_f.write("#define REG_%s_ADDR\t%s\r\n"%(row['Name'].upper(), hex(addr)))
		rmh_f.write("#define REG_%s_MIN\t%d\r\n"%(row['Name'].upper(), min))
		rmh_f.write("#define REG_%s_MAX\t%d\r\n"%(row['Name'].upper(), max))
		rmh_f.write("#define REG_%s_DEF\t%d\r\n"%(row['Name'].upper(), default))
		rmh_f.write("#define REG_%s_OPER\t%d\r\n"%(row['Name'].upper(), oper))
		rmh_f.write("\r\n")
			
	rmh_f.write("#define REG_LAST_ADDR\t%d /*Last register address*/\r\n"%(last_reg_addr))
	rmh_f.write("#define REG_NUM\t\t\t%d /*Total registers number*/\r\n"%(reg_num))
	rmh_f.write("\r\n")
	
	rmh_f.write("#endif /*REG_MAP_H_*/\r\n")
	rmh_f.write("\r\n")
		
	print("File reg_map.h is created")
	
	'''Registers functions source file'''
	mbr_f = open('mb_regs.c', 'w', newline='')
	#Comment
	mbr_f.write("/**\r\n* This file is created automatically\r\n")
	mbr_f.write("* Created on: %s\r\n**/\r\n"%(datetime.date.today()))
	mbr_f.write("\r\n")
	#includes
	mbr_f.write("#include \"reg_map.h\"\r\n")
	mbr_f.write("\r\n")
	#typedefs
	mbr_f.write("typedef enum {REG_READ = 1, REG_WRITE} RegOpMode;\r\n")
	mbr_f.write("\r\n")
	#static variables
	mbr_f.write("static uint16_t MBRegs[REG_NUM] = {\r\n")
	for row in reg_map:
		mbr_f.write("\t%s,\t/*%s*/\r\n"%(row['Default'], row['Name'].upper()))
	mbr_f.write("}\r\n")
	mbr_f.write("\r\n")
	#function prototypes
	mbr_f.write("uint32_t RegCheckOp(uint16_t addr, RegOpMode op);\r\n")
	mbr_f.write("uint32_t RegCheckVal(uint16_t addr, uint16_t val);\r\n")
	mbr_f.write("\r\n")
	
	#functions
	mbr_f.write("/*Check register operation permission*/\r\n")
	mbr_f.write("uint32_t RegCheckOp(uint16_t addr, RegOpMode op)\r\n")
	mbr_f.write("{\r\n")
	for row in reg_map:
		mbr_f.write("\tif (addr == REG_%s_ADDR && !(op & REG_%s_OPER)) return 0;\r\n"%(row['Name'].upper(),row['Name'].upper()))
	mbr_f.write("\r\n")
	mbr_f.write("\treturn 1;\r\n")
	mbr_f.write("}\r\n")
	
	mbr_f.write("/*Checks register values restrictions*/\r\n")
	mbr_f.write("uint32_t RegCheckVal(uint16_t addr, uint16_t val)\r\n")
	mbr_f.write("{\r\n")
	for row in reg_map:
		mbr_f.write("\tif (addr == REG_%s_ADDR && (val < REG_%s_MIN || val > REG_%s_MAX)) return 0;\r\n"%(row['Name'].upper(),row['Name'].upper(),row['Name'].upper()))
	mbr_f.write("\r\n")
	mbr_f.write("\treturn 1;\r\n")
	mbr_f.write("}\r\n")
	
	print("File mb_regs.c is created")
			
if __name__ == "__main__":
	sys.exit(main())