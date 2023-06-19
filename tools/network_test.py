#!/usr/bin/env python3

import socket
import argparse
import random
import string
import time
import sys
import signal

def sig_handler(signum, frame):
    global count
    print(f'Got {count} correct responses')
    sys.exit(0)

def run_test(args):

    global count
    sock = socket.socket(socket.AF_INET,
                         socket.SOCK_STREAM)
    addr = (args.address, args.port)
    sock.connect(addr)

    while count < args.num or args.num < 0:
        msg = ''.join(random.choices(string.ascii_uppercase + string.digits, k=args.size)).encode('UTF-8')

        sock.sendall(msg)

        time.sleep(args.delay)

        reply = sock.recv(args.size)

        if len(reply) != args.size:
            print(f'Incorrect return size {args.size} vs. {len(reply)}')
            if not args.ignore:
                sys.exit(1)
        
        if reply != msg:
            print(f'Incorrect reply {msg} vs. {reply}')
            if not args.ignore:
                sys.exit(1)
        
        count += 1

def main():
    parser = argparse.ArgumentParser()

    parser.add_argument('--size', '-s',default=64 , type=int)
    parser.add_argument('--num', '-n',default=64 , type=int)
    parser.add_argument('--port', '-p',default=4455, type=int)
    parser.add_argument('--address', '-a',default='10.0.2.15', type=str)
    parser.add_argument('--delay', '-d',default=.1, type=float)
    parser.add_argument('--ignore', '-i',action='store_true')
    
    args = parser.parse_args()

    signal.signal(signal.SIGINT, sig_handler)

    run_test(args)

    sys.exit(0)

count = 0

if __name__ == '__main__':
    main()