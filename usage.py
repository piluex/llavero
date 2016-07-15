import hashlib
import binascii
import serial
import time
import getpass
import cmd 

def waitACK(ll):
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

class LLAVEROShell(cmd.Cmd):
	intro = 'Hello world. Type help or ? to list commands.\n'
	prompt = '(LLAVERO) '
	LL = None

	def __init__(self, ll = None):
		cmd.Cmd.__init__(self)
		self.LL = ll

	def do_set(self, arg):
		'set [tag] -> secret prompt. [tag] is 7 char max.'
		self.LL.write('set\n')
		waitACK(self.LL)
		self.LL.write('{0}\n'.format(arg))
		waitACK(self.LL)
		prompt = self.LL.readline()
		secret = getSecret(prompt)
		self.LL.write('{0}\n'.format(secret))
		del secret
		waitACK(self.LL)
	def do_get(self, arg):
		'get [tag]. [tag] is 7 char max.'
		self.LL.write('get\n')
		waitACK(self.LL)
		self.LL.write('{0}\n'.format(arg))
		print self.LL.readline()
	def do_ls(self, arg):
		'Lists all tags.'
		self.LL.write('ls\n')
		waitACK(self.LL)
		print self.LL.readline()
	def do_init(self, arg):
		'Factory reset, push confirmation.'
		self.LL.write('init\n')
		waitACK(self.LL)
		print self.LL.readline()
	def do_secret(self, arg):
		'Set the AES key, this scripts derivates a plain text for you.'
		self.LL.write('secret\n')
		waitACK(self.LL)
		prompt = LLAVERO.readline()
		secret = getSecret(prompt)
		#Note to future me:
		#The salt and the number should be picked by each user as a safety
		#just in case the secret is evasedroped and the LLAVERO stolen, with no
		#salt+number the secret is not that useful.
		d_secret_pi = hashlib.pbkdf2_hmac('sha256', secret, b'sal', 314159)
		del secret
		LLAVERO.write('0x{0}\n'.format(binascii.hexlify(d_secret_pi)))
		del d_secret_pi
		waitACK(LLAVERO)
		print 'Secret sent.'


if __name__ == '__main__':
	print "Contacting LLAVERO..."
	#port = '/dev/ttyUSB0'#linux
	port = '/dev/cu.usbmodemHIDP1'#osx
	with serial.Serial(port, 9600, timeout=2) as LLAVERO:
		time.sleep(1)
		LLAVERO.write('hi\n');
		waitACK(LLAVERO)
		print LLAVERO.readline()
		LLAVEROShell(LLAVERO).cmdloop()
