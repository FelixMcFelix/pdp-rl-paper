class Quantiser:
	"""docstring for Quantiser"""
	def __init__(self, num, denom):
		self.num = float(num)
		self.denom = float(denom)

	@staticmethod
	def binary(power):
		return Quantiser(1 << power, 1)

	def into(self, value):
		return int((value * self.num) / self.denom)

	def from_q(self, value):
		return (float(value) * self.denom) / self.num
	
	def mul(self, lhs, rhs):
		return (lhs * rhs * int(self.denom)) / int(self.num)

	def div(self, lhs, rhs):
		return ((lhs / rhs) * int(self.num)) / int(self.denom)
