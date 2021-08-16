#!/usr/bin/python
# encoding: utf-8

import sys
import csv
import datetime
import string

from argparse import ArgumentParser

REG_MIN_VALUE = 0
REG_MAX_VALUE = 65535

def main(argv=None): # IGNORE:C0111
    '''Command line options.'''

    if argv is None:
        argv = sys.argv
    else:
        sys.argv.extend(argv)
    
    try:
        # Setup argument parser
        parser = ArgumentParser(description="Register map generator.")
        parser.add_argument("file", help=".csv input file")

        # Process arguments
        args = parser.parse_args()
        
        file = args.file
        
    except KeyboardInterrupt:
        ### handle keyboard interrupt ###
        sys.exit(0)
    except Exception as e:
        sys.exit(-1)
 
    with open(file, newline='') as csvfile:
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
                
                reg_map.append(row)
                if addr > last_reg_addr:
                    last_reg_addr = addr
        except csv.Error as e:
            sys.exit('file {}, line {}: {}'.format(filename, reader.line_num, e))
            
        print("Last Address: %s"%(hex(last_reg_addr)))
        reg_num = last_reg_addr + 1
        print("Registers number: %s"%(reg_num))
        
        reg_map = sorted(reg_map, key=lambda k: k['Address']) 
        
    '''Create C files'''
        
    '''Register map'''
    #open template file
    with open("mb_regs_h.template") as ht:
        rmh_template = string.Template(ht.read())
        
    rmh_f = open('mb_regs.h', 'w', newline='')
    
    reg_map_defs = ""
    
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
            
        reg_map_defs += "/* Register: %s\r\n* Addr: %s; Min: %d; Max: %d; Default: %d\r\n*/\r\n"%(row['Comment'], hex(addr), min, max, default)
        reg_map_defs += "#define REG_%s_ADDR\t%s\r\n"%(row['Name'].upper(), hex(addr))
        reg_map_defs += "#define REG_%s_MIN\t%d\r\n"%(row['Name'].upper(), min)
        reg_map_defs += "#define REG_%s_MAX\t%d\r\n"%(row['Name'].upper(), max)
        reg_map_defs += "#define REG_%s_DEF\t%d\r\n"%(row['Name'].upper(), default)
        reg_map_defs += "#define REG_%s_OPER\t%d\r\n"%(row['Name'].upper(), oper)
        reg_map_defs += "\r\n"
    
    #fill template and write to file        
    rmh_content = rmh_template.safe_substitute(date=datetime.date.today(), \
                                               register_map = reg_map_defs, \
                                               reg_last_addr = hex(last_reg_addr), \
                                               reg_num = reg_num)
    rmh_f.write(rmh_content)
    
    print("File mb_regs.h is created")
    
    '''Registers functions source file'''
    #open template file
    with open("mb_regs_c.template") as ht:
        mbr_template = string.Template(ht.read())
        
    mbr_f = open('mb_regs.c', 'w', newline='')

    #static variables
    #fill geristers array
    reg_def_vals = ""
    for row in reg_map:
        reg_def_vals += "\t%s"%(row['Default'])
        if row != reg_map[-1]:
            reg_def_vals += ","
            reg_def_vals += "\t/*%s*/\r\n"%(row['Name'].upper())
        else:
            reg_def_vals += "\t/*%s*/"%(row['Name'].upper())
     
    #checker functions
    reg_op_check = ""
    for row in reg_map:
        reg_op_check += "\tif (addr == REG_%s_ADDR && !(op & REG_%s_OPER)) return 0;\r\n"%(row['Name'].upper(),row['Name'].upper())
    
    reg_val_check = ""
    for row in reg_map:
        reg_val_check += "\tif (addr == REG_%s_ADDR && (val < REG_%s_MIN || val > REG_%s_MAX)) return 0;\r\n"%(row['Name'].upper(),row['Name'].upper(),row['Name'].upper())
    
    #fill template and write to file        
    mbr_content = mbr_template.safe_substitute(date=datetime.date.today(), \
                                               def_vals = reg_def_vals, \
                                               op_check = reg_op_check, \
                                               val_check = reg_val_check)
    mbr_f.write(mbr_content)
    
    print("File mb_regs.c is created")
            
if __name__ == "__main__":
    sys.exit(main())
