import string
import random

def main():
	f = open("input.txt", "w")
	chars = string.ascii_uppercase + string.digits
	for i in range(1000):
		id = str(i)
		datalen = 1024 - 2 - len(id)
		data = ''.join(random.choice(chars) for _ in range(datalen))
		f.write(id + " " + data + "\n")
	f.close()

if __name__ == "__main__":
	main()

