class Quantiser:
	"""docstring for Quantiser"""
	def __init__(self, num, denom):
		self.num = float(num)
		self.denom = float(denom)

	def binary(power):
		return Quantiser(1, 1 << power)

	def into(self, value):
		return int((value * num) / denom)

	def from_q(self, value):
		return (float(value) * denom) / num
	
	def mul(self, lhs, rhs):
		(lhs * rhs * int(self.denom)) / int(self.num)

	def div(self, lhs, rhs):
		((lhs / rhs) * int(self.num)) / int(self.denom)
