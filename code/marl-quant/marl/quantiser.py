import numpy as np

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

def quant_dtype_for_frac_len(q_sz):
	#basic rules: copy the rust program?
	# 1 sign bit.
	# q_sz = mantissa

	spent_bits = 1 + q_sz
	# qdt, req'd whole part bits, bits avail in this format.
	quantiser_choices = [
		(np.dtype("i1"), 2, 8),
		(np.dtype("i2"), 5, 16),
		(np.dtype("i4"), 10, 32),
		(np.dtype("i8"), 21, 64),
	]

	for (qdt, whole_part, q_max) in quantiser_choices:
		if spent_bits + whole_part <= q_max:
			return qdt

	print("Q_SZ:{} TOO LARGE TO FIT IN 64b NUMBER.".format(q_sz))
	return None
