#!/usr/bin/python
# encoding: utf-8

import sys
import csv
import datetime
import string

from argparse import ArgumentParser
from rich.console import Console
from rich.table import Table

REG_MIN_VALUE = 0
REG_MAX_VALUE = 0xFFFF

#Converts string to int value
def str_field2int(field):
    try:
        if field.startswith('0x'):
            val = int(field.replace("0x",""), 16)
        else:
            val = int(field)
    except ValueError:
        val = -1
    return val

#Adds separation between str1 and str2 to aling str2 to pos
def tab2pos(str1, str2, pos):
    out_str = str1;
    for i in range(pos - len(str1)):
        out_str += ' ';
    out_str += str2;
    return out_str;

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
        max_name_len = 0
        reg_map = []

        console = Console()
        
        console.print("Check input file")
        
        '''Check CSV table content'''
        try:
            for row in reader:
                if row['Name'].upper() == 'RESERVED':
                    console.print("[yellow]Warning: Skip RESERVED register")
                    continue
                
                addr = str_field2int(row['Address'])
                if addr < REG_MIN_VALUE or addr > REG_MAX_VALUE:
                    addr = 0
                    console.print("[yellow]Warning: Address of register \"%s\" is not correct. Set to 0"%(row['Name']))
                    
                min = str_field2int(row['Min'])
                if min < REG_MIN_VALUE or min > REG_MAX_VALUE:
                    min = 0
                    console.print("[yellow]Warning: Minimum value of register \"%s\" is not correct. Set to 0"%(row['Name']))
                    
                max = str_field2int(row['Max'])
                if max < REG_MIN_VALUE or max > REG_MAX_VALUE:
                    max = 0xFFFF
                    console.print("[yellow]Warning: Maximum value of register \"%s\" is not correct. Set to 0xFFFF"%(row['Name']))
                    
                if min >= max:
                    console.print("[yellow]Warning: Minimum value of register \"%s\" is equal or greiter than maximum value"%(row['Name']))
                    
                default = str_field2int(row['Default'])
                if default < REG_MIN_VALUE or default > REG_MAX_VALUE:
                    default = 0
                    console.print("[yellow]Warning: Default value of register \"%s\" is not correct. Set to 0"%(row['Name']))
                    
                if row['Mode'].upper() == 'R' or row['Mode'].upper() == 'W' or row['Mode'].upper() == 'RW' or row['Mode'].upper() == 'R/W':
                    oper = row['Mode'].upper()
                else:
                    oper = 'R'
                    console.print("[yellow]Warning: Mode of register \"%s\" is not correct. Set to R"%(row['Name']))
         
                reg_map.append({'Address':addr, 'Min':min, 'Max':max, 'Default':default, 'Mode':oper, 'Name':row['Name'], 'Comment':row['Comment']})
                
                if addr > last_reg_addr:
                    last_reg_addr = addr
                    
                if len(row['Name']) > max_name_len:
                    max_name_len = len(row['Name'])

        except csv.Error as e:
            sys.exit('file {}, line {}: {}'.format(filename, reader.line_num, e))
            
        console.print("Last Address: %s"%(hex(last_reg_addr)))
        reg_num = last_reg_addr + 1
        console.print("Registers number: %s"%(reg_num))
        
        reg_map = sorted(reg_map, key=lambda k: k['Address'])

        '''Create Table'''
        table = Table(title="[bold]Registers map")

        table.add_column("Address", style="cyan", no_wrap=True)
        table.add_column("Caption", style="green")
        table.add_column("Min")
        table.add_column("Max")
        table.add_column("Default")
        table.add_column("Mode")

        for row in reg_map:
            #Fill Console table
            table.add_row(hex(row['Address']), row['Name'], str(row['Min']), str(row['Max']), str(row['Default']), row['Mode'])

        console.print(table)
        
        '''Create C files'''
            
        '''Register map'''
        #open template file
        with open("mb_regs_h.template") as ht:
            rmh_template = string.Template(ht.read())
            
        rmh_f = open('mb_regs.h', 'w', newline='')
        
        reg_map_defs = ""
        #get closest tab position index
        tab_pos_ind = 4*((max_name_len+ 17 + 3)//4) + 4;
        
        for row in reg_map:
            reg_name = row['Name'].upper()
            reg_name = reg_name.replace(' ','_')
            
            if reg_name == 'RESERVED':
                continue
            
            addr = row['Address']
            min = row['Min']
            max = row['Max']
            default = row['Default']
            oper = row['Mode']
            
            if oper == 'R':
                oper_mode = 'REG_OPT_R_ONLY'
            elif oper == 'W':
                oper_mode = 'REG_OPT_WR_ONLY'
            elif oper == 'RW' or oper == 'R/W':
                oper_mode = 'REG_OPT_ALL'
                
            reg_map_defs += "/* Register: %s\r\n* Addr: %s; Min: %d; Max: %d; Default: %d; Oper: %s */\r\n"%(row['Comment'], hex(addr), min, max, default, oper)
            reg_map_defs += tab2pos("#define REG_%s_ADDR"%(reg_name), "%s\r\n"%(hex(addr)), tab_pos_ind)
            reg_map_defs += tab2pos("#define REG_%s_MIN"%(reg_name), "%s\r\n"%(min), tab_pos_ind)
            reg_map_defs += tab2pos("#define REG_%s_MAX"%(reg_name), "%s\r\n"%(max), tab_pos_ind)
            reg_map_defs += tab2pos("#define REG_%s_DEF"%(reg_name), "%s\r\n"%(default), tab_pos_ind)
            reg_map_defs += tab2pos("#define REG_%s_OPT"%(reg_name), "%s\r\n"%(oper_mode), tab_pos_ind)
            reg_map_defs += "\r\n"
        
        #fill template and write to file
        rmh_content = rmh_template.safe_substitute(date=datetime.date.today(), \
                                                   register_map = reg_map_defs, \
                                                   reg_last_addr = hex(last_reg_addr), \
                                                   reg_num = reg_num)
        rmh_f.write(rmh_content)
        
        console.print("[green]File mb_regs.h is created")
        
        '''Registers functions source file'''
        #open template file
        with open("mb_regs_c.template") as ht:
            mbr_template = string.Template(ht.read())
            
        mbr_f = open('mb_regs.c', 'w', newline='')

        #static variables
        
        #fill geristers options array
        reg_opts_vals = ""
        for row in reg_map:
            reg_name = row['Name'].upper()
            reg_name = reg_name.replace(' ','_')
            
            reg_opt_str = ''
            
            if reg_name == 'RESERVED':
                reg_opt_str += "\t{REG_OPT_R_ONLY, 0, 0, 0}"
            else:
                reg_opt_str += \
                "\t{REG_%s_OPT, REG_%s_MIN, REG_%s_MAX, REG_%s_DEF}"%(reg_name, \
                                                                      reg_name, \
                                                                      reg_name, \
                                                                      reg_name)
            if row != reg_map[-1]:
                reg_opt_str += ",\r\n"
            
            reg_opts_vals += reg_opt_str
        
        #fill geristers values array
        reg_def_vals = ""
        for row in reg_map:
            reg_name = row['Name'].upper()
            reg_name = reg_name.replace(' ','_')
            
            if reg_name == 'RESERVED':
                reg_def_vals += "\t0"
            else:
                reg_def_vals += "\tREG_%s_DEF"%(reg_name)
            if row != reg_map[-1]:
                reg_def_vals += ",\r\n"
         
        #fill template and write to file
        mbr_content = mbr_template.safe_substitute(date=datetime.date.today(), \
                                                   opt_vals = reg_opts_vals, \
                                                   def_vals = reg_def_vals)
        mbr_f.write(mbr_content)
        
        console.print("[green]File mb_regs.c is created")
            
if __name__ == "__main__":
    sys.exit(main())
