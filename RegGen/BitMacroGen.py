#!/usr/bin/python
# encoding: utf-8

import sys
import csv

from argparse import ArgumentParser
from rich.console import Console
from rich.table import Table
from rich import box

REG_MIN_VALUE = 0
REG_MAX_VALUE = 0xFFFF
DEFAULT_BIT_LEN = 16
STR2SKIP = ["","-","RESERVED"]

#Adds separation between str1 and str2 to aling str2 to pos
def tab2pos(str1, str2, pos):
    out_str = str1
    for _ in range(pos - len(str1)):
        out_str += ' '
    out_str += str2
    return out_str

def main(argv=None): # IGNORE:C0111
    '''Command line options.'''

    if argv is None:
        argv = sys.argv
    else:
        sys.argv.extend(argv)
    
    try:
        # Setup argument parser
        parser = ArgumentParser(description="Bit manipulation macro generator.")
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
        max_name_len = 0
        items = []
        bit_keys = ['Bit %i'%(n) for n in range(DEFAULT_BIT_LEN)]

        console = Console()
        
        console.print("Check input file")

        ''' Check CSV table content '''
        if not ('Name' in reader.fieldnames):
            console.print("The table does not contain a key \"Name\"")
        for k in bit_keys: 
            if not (k in reader.fieldnames):
                console.print("The table does not contain a key \"%s\""%(k))

        try:
            for row in reader:
                if row['Name'].upper() == 'RESERVED':
                    console.print("[yellow]Warning: Skip RESERVED")
                    continue
                
                prew_bit_str = ""
                bit_len = 0
                bit_start = 0
                for b in range(len(bit_keys)):
                    curr_bit_str = row[bit_keys[b]].upper()
                    if curr_bit_str != prew_bit_str:
                        if curr_bit_str in STR2SKIP:
                            if not (prew_bit_str in STR2SKIP):
                                if bit_len > 0:
                                    items.append({
                                                    'Reg name' : row['Name'].upper(),
                                                    'Field name' : prew_bit_str,
                                                    'Start': bit_start,
                                                    'Width': bit_len,
                                                })
                            bit_len = 0
                        else:
                            if bit_len > 0:
                                items.append({
                                                'Reg name' : row['Name'].upper(),
                                                'Field name' : prew_bit_str,
                                                'Start': bit_start,
                                                'Width': bit_len,
                                            })

                            bit_len = 1
                            bit_start = b
                                
                    else:
                        if not (curr_bit_str in STR2SKIP):
                            bit_len = bit_len + 1

                    if (b == len(bit_keys) - 1) and not (curr_bit_str in STR2SKIP):
                        items.append({
                                        'Reg name' : row['Name'].upper(),
                                        'Field name' : curr_bit_str,
                                        'Start': bit_start,
                                        'Width': bit_len,
                                    })

                    prew_bit_str = curr_bit_str

        except csv.Error as e:
            sys.exit('file {}, line {}: {}'.format(filename, reader.line_num, e))

        for i in items:
            if len(i['Reg name']+"_"+i['Field name']) > max_name_len:
                max_name_len = len(i['Reg name']+"_"+i['Field name'])
            
        ''' Create Table '''
        table = Table(box=box.SIMPLE_HEAD,
                        row_styles=["none", "dim"],
                        pad_edge=False)

        table.add_column("Reg name", style="cyan", no_wrap=True)
        table.add_column("Field name", style="green")
        table.add_column("Start bit")
        table.add_column("Bit len")

        for i in items:
            #Fill Console table
            table.add_row(i['Reg name'], i['Field name'], str(i['Start']), str(i['Width']))

        console.print(table)
        
        ''' Create C files '''
        bit_macro_txt = ""
        #get closest tab position index
        tab_pos_ind = 4*((max_name_len + 18 + 3)//4) + 4

        for i in items:
            prefix = i['Reg name'] + "_" + i['Field name']
            prefix = prefix.replace(' ','_')
            start = i['Start']
            width = i['Width']

            bit_macro_txt += "/* Register: %s; Field: %s */\r\n"%(i['Reg name'], i['Field name'])

            ''' Mask '''
            mask = 0x0000
            for i in range(width):
                mask = mask | (1 << i)
            mask = mask << start
            mask_macro_str = "%s_MASK"%(prefix)

            bit_macro_txt += tab2pos("#define " + mask_macro_str, "(0x%04XU)\r\n"%(mask), tab_pos_ind)

            ''' Getter '''
            getter_str = "((a) & " + mask_macro_str + ")"
            if start > 0:
                getter_str = "(" + getter_str + " >> %i"%(start) + ")"
            bit_macro_txt += tab2pos("#define %s_GET(a)"%(prefix), getter_str+"\r\n", tab_pos_ind)

            ''' Setter '''
            setter_str = "(a) = ((a) & ~" + mask_macro_str + ")"
            if start > 0:
                setter_str = setter_str + " | ((b) << %i)"%(start)
            else:
                setter_str = setter_str + " | (b)"
            bit_macro_txt += tab2pos("#define %s_SET(a, b)"%(prefix), setter_str+"\r\n", tab_pos_ind)

            bit_macro_txt += "\r\n"
        
        bmh_f = open('bit_macro.h', 'w', newline='')
        bmh_f.write(bit_macro_txt)
        console.print("[green]File bit_macro.h is created")
               
if __name__ == "__main__":
    sys.exit(main())
