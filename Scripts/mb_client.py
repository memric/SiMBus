#!/usr/bin/env python

import sys
import argparse
from argparse import ArgumentParser
from MBClient import MBClient
from rich.console import Console
from rich.table import Table
from rich.live import Live
from time import sleep

def register_table(reg_values) -> Table:
    table = Table()

    table.add_column("Address", style="cyan", no_wrap=True)
    table.add_column("Caption", style="green")
    table.add_column("Raw value")
    table.add_column("Converted value")

    return table

def main(argv=None):
    if argv is None:
        argv = sys.argv
    else:
        sys.argv.extend(argv)
    
    try:
        # Setup argument parser
        parser = ArgumentParser(description = 'Simple ModBus client.',
        formatter_class=argparse.RawTextHelpFormatter)
        parser.add_argument('device', help='Serial device', metavar='DEV')
        parser.add_argument('func',
        type=int, 
        help='''Modbus function.
        1 - Read Coil Status
        2 - Read Input Status
        3 - Read Holding Registers
        4 - Read Input Registers
        5 - Force Single Coil
        6 - Force Single Register
        15 - Force Multiple Coils
        16 - Preset Multiple Registers''',
        metavar='FUNC')
        parser.add_argument('-s', '--start', default=0, help='Start address')
        parser.add_argument('-n', '--num', default=1, help='Points number')
        parser.add_argument('-a', '--addr', dest='addr', default=1, help='Server Modbus address')
        parser.add_argument('-p', '--poll', dest='poll', action='store_true', help='Poll registers')
        parser.add_argument('-t', '--timeout', dest='timeout', default=1000, help='Polling timeout')
        parser.add_argument('--version', action='version', version='0.0.1')
        
        # Process arguments
        args = parser.parse_args()
        
        #connect to controller
        reader = MBClient(args.device, int(args.addr))
        timeout = int(args.timeout)

        #Check function
        if not(args.func in [1, 2, 3, 4, 5, 6, 15, 16]):
            sys.stderr.write("Incorrect function code. For help use --help")
        
        console = Console()

        # with Live(console=console, screen=False, auto_refresh=False) as live:
        #     while True:
        #         reg_values = reader.status()
        #         live.update(register_table(reg_values), refresh=True)

        #         if args.poll == False:
        #             break
        #         else:
        #             sleep(timeout/1000)
            
        return 0
                
    except KeyboardInterrupt:
        ### handle keyboard interrupt ###
        return 0
    except Exception as e:
        sys.stderr.write(" for help use --help")
        return 2
        
if __name__ == "__main__":
    sys.exit(main())
