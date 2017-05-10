""" test the confundo server """

import socket
import argparse
from ctypes import Structure, c_uint32, c_uint16

parser = argparse.ArgumentParser(description='test the confundo server')
parser.add_argument('UDP_HOST', type=str, help='hostname or IP to connect to')
parser.add_argument('UDP_PORT', type=int, help='port to connect to')
parser.add_argument('FILE', type=str, help='name of file to send')
args = parser.parse_args()

class ConfundoHeader(Structure):
    _fields_ = [
        ('seq', c_uint32),
        ('ack', c_uint32),
        ('id', c_uint16),
        ('xxx', c_uint16, 13),
        ('ackf', c_uint16, 1),
        ('synf', c_uint16, 1),
        ('finf', c_uint16, 1)
    ]

class ConfundoClient:
    def __init__(self, UDP_IP, UDP_PORT):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.connect((UDP_IP, UDP_PORT))

    def sendfile(self, filename):
        pass

    def __connect(self):
        # send syn
        pass

if __name__ == '__main__':
    c = ConfundoClient(args.UDP_HOST, args.UDP_IP)
    c.sendfile(args.FILE)
