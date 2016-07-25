#!/usr/bin/python
import hashlib
import binascii
import serial
import time
import calendar
import getpass
import cmd
import base64

def waitACK(ll):
	time.sleep(0.1)
	ack = None
	while ack is None:
		ack = ll.readline()
	if ack != 'ACK\n':
		print 'Expected ACK, got: {0}'.format(ack)
		exit(1)

def getSecret(prompt):
	good = False
	secret_a = None
	while not good:
		secret_a = getpass.getpass(prompt)
		secret_b = getpass.getpass('Confirm:\n')
		good = secret_a == secret_b
	return secret_a

def resetInput(ll):
	time.sleep(0.1)
	ll.reset_input_buffer()

class LLAVEROShell(cmd.Cmd):
	intro = 'Hello world. Type help or ? to list commands.\n'
	prompt = '(LLAVERO) '
	LL = None

	def __init__(self, ll = None):
		cmd.Cmd.__init__(self)
		self.LL = ll
	def do_hi(self, arg):
		'Say hi.'
		self.LL.write('hi\n')
		waitACK(self.LL)
		print self.LL.readline()
	def do_set(self, arg):
		'set [tag] -> secret prompt. [tag] is 7 char max.'
		self.LL.write('set\n')
		waitACK(self.LL)
		self.LL.readline()
		self.LL.write('{0}\n'.format(arg))
		waitACK(self.LL)
		print self.LL.readline()
		prompt = self.LL.readline()
		secret = getSecret(prompt)
		self.LL.write('{0}\n'.format(secret))
		#I know, del is no good, for memory control python is no good.
		del secret
		waitACK(self.LL)
	def do_get(self, arg):
		'get [tag]. [tag] is 7 char max.'
		self.LL.write('get\n')
		waitACK(self.LL)
		self.LL.readline()#ENTER TAG
		self.LL.write('{0}\n'.format(arg))
		waitACK(self.LL)
		print self.LL.readline()
	def do_ls(self, arg):
		'Lists all tags.'
		self.LL.write('ls\n')
		waitACK(self.LL)
		print self.LL.readline()
		time.sleep(1)
		result = None
		while result is None or len(result) < 1:
			result = self.LL.read(255)
		print result
	def do_init(self, arg):
		'Factory reset, push confirmation.'
		self.LL.write('init\n')
		waitACK(self.LL)
		print self.LL.readline()
	def do_secret(self, arg):
		'Set the AES key, this scripts derivates a plain text for you.'
		self.LL.write('secret\n')
		waitACK(self.LL)
		prompt = self.LL.readline()
		secret = getSecret(prompt)
		#Note to future me:
		#The salt and the number should be picked by each user as a safety
		#just in case the secret is evasedroped and the LLAVERO stolen, with no
		#salt+number the secret is not that useful.
		d_secret_pi = hashlib.pbkdf2_hmac('sha256', secret, b'sal', 314159)
		del secret
		self.LL.write('0x{0}\n'.format(binascii.hexlify(d_secret_pi)))
		print 'DEBUG SECRET: 0x{0}\n'.format(binascii.hexlify(d_secret_pi))#debug
		del d_secret_pi
		waitACK(self.LL)
		print 'Secret sent.'
	def do_time(self, arg):
		'Tells what time is it to LLAVERO. LLAVERO needs to know that for TOTP codes.'
		self.LL.write('time\n')
		waitACK(self.LL)
		self.LL.readline()
		self.LL.write(hex(calendar.timegm(time.gmtime()))+"\n")
		waitACK(self.LL)
		time.sleep(0.1)
		print self.LL.readline()
	def do_sett(self, tag):
		'sett [tag]. Sets [tag] for totp, the secret is expected in base32 google format.'
		self.LL.write('sett\n')
		waitACK(self.LL)
		self.LL.readline()
		self.LL.write('{0}\n'.format(tag))
		prompt = self.LL.readline()
		print prompt
		arg = getSecret(prompt)
		parsed = arg.replace(' ', '').upper()
		bin = base64.b32decode(parsed)
		nice_hex = '0x{0}\n'.format(binascii.hexlify(bin))
		print 'Debug: ' + nice_hex
		self.LL.write(nice_hex)
		waitACK(self.LL)
		print 'Secret sent.'
	def precmd(self, line):
		resetInput(self.LL)
		return line

if __name__ == '__main__':
	print "Contacting LLAVERO..."
	#port = '/dev/ttyUSB0'#linux
	port = '/dev/cu.usbmodemHIDP1'#osx
	with serial.Serial(port, 9600, timeout=1) as LLAVERO:
		time.sleep(2)
		LLAVERO.write('hi\n');
		waitACK(LLAVERO)
		print LLAVERO.readline()
		LLAVEROShell(LLAVERO).cmdloop()
