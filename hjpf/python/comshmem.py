import os
import posix_ipc as ipc
import configparser
import mmap
import struct
import numpy as np
import syslog
from enum import Enum

class ShmemKind(Enum):	# 種別
	NONE = 0
	PLATFORM = 1
	USER = 2

class ProcStat:
	num = 0
	stat = [i for i in range(128)]
	cpu = [i for i in range(128)]
	mem = [i for i in range(128)]

	def fromByte(self, bytes):	#バイトを整数に変換
		pos = 0
		#プロセス数
		self.num = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos+=4

		#死活情報
		for i in range(self.num):
			self.stat[i] = int.from_bytes(bytes[pos:pos+4], byteorder='little')
			pos+=4

		pos = 516
		for i in range(self.num):
			self.cpu[i] = struct.unpack('<f', bytes[pos:pos+4])[0]
			pos+=4

		pos = 1028
		for i in range(self.num):
			self.mem[i] = struct.unpack('<f', bytes[pos:pos+4])[0]
			pos+=4
		
	def toByte(self):
		#プロセス数
		byte = self.num.to_bytes(4, byteorder='little')

		#死活情報
		for i in range(128):
			byte += self.stat[i].to_bytes(4, byteorder='little')

		for i in range(128):
			byte += struct.pack('<f', self.cpu)

		for i in range(128):
			byte += struct.pack('<f', self.mem)

		return byte

class ResStat:
	cpu_load = [i for i in range(13)]
	mem_load = 0
	disk_load = 0
	cpu_therm = 0
	cpu_num = 12

	def fromByte(self, bytes):
		pos = 0
		#CPU使用率
		for i in range(self.cpu_num + 1):
			self.cpu_load[i] = int.from_bytes(bytes[pos:pos+4], byteorder='little')
			pos += 4

		#メモリ使用率
		self.mem_load = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos+=4
		#ディスク使用率
		self.disk_load = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos+=4
		#CPU温度
		self.cpu_therm = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos+=4

	def toByte(self):
		byte = bytes()
		#CPU使用率
		for i in range(self.cpu_num + 1):
			byte += self.cpu_load[i].to_bytes(4, byteorder='little')

		#メモリ使用率
		byte += self.mem_load.to_bytes(4, byteorder='little')
		#ディスク使用率
		byte += self.disk_load.to_bytes(4, byteorder='little')
		#CPU温度
		byte += self.cpu_therm.to_bytes(4, byteorder='little')

		return byte

class GNSS():
	stat = 0
	
	gga_time = 0
	gga_latitude = 0
	gga_latitude_sign = 0
	gga_longitude = 0
	gga_longitude_sign = 0
	gga_mode_status = 0
	gga_satelite_num = 0
	gga_hdop = 0
	gga_height_sea = 0
	gga_height_geoid = 0

	rmc_time = 0
	rmc_latitude = 0
	rmc_latitude_sign = 0
	rmc_longitude = 0
	rmc_longitude_sign = 0
	rmc_knots = 0
	rmc_date = 0
	rmc_azimuth = 0
	rmc_mag_dec = 0
	rmc_mag_dec_dir = 0
	rmc_mode_status = ''

	gsa_pdop = 0
	gsa_hdop = 0

	def fromByte(self, bytes):
		pos = 0
		#通信状況
		self.stat = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos+=4
		#GGA時刻
		self.gga_time = struct.unpack('<d', bytes[pos:pos+8])[0]
		pos+=8
		#GGA緯度
		self.gga_latitude = struct.unpack('<d', bytes[pos:pos+8])[0]
		pos+=8
		#GGA北緯/南緯
		self.gga_latitude_sign = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos+=4
		#GGA経度
		self.gga_longitude = struct.unpack('<d', bytes[pos:pos+8])[0]
		pos+=8
		#GGA東経/西経
		self.gga_longitude_sign = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos+=4
		#GGA測位モードステータス
		self.gga_mode_status = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos+=4
		#GGA衛星数
		self.gga_satelite_num = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos+=4
		#GGAHDOP
		self.gga_hdop = struct.unpack('<d', bytes[pos:pos+8])[0]
		pos+=8
		#GGA海抜高度
		self.gga_height_sea = struct.unpack('<d', bytes[pos:pos+8])[0]
		pos+=8
		#ジオイド高度
		self.gga_height_geoid = struct.unpack('<d', bytes[pos:pos+8])[0]
		pos+=8

		self.rmc_time = struct.unpack('<d', bytes[pos:pos+8])[0]
		pos+=8
		self.rmc_latitude = struct.unpack('<d', bytes[pos:pos+8])[0]
		pos+=8
		self.rmc_latitude_sign = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos+=4
		self.rmc_longitude = struct.unpack('<d', bytes[pos:pos+8])[0]
		pos+=8
		self.rmc_longitude_sign = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos+=4
		self.rmc_knots = struct.unpack('<d', bytes[pos:pos+8])[0]
		pos+=8
		self.rmc_date = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos+=4
		self.rmc_azimuth = struct.unpack('<d', bytes[pos:pos+8])[0]
		pos+=8
		self.rmc_mag_dec = struct.unpack('<d', bytes[pos:pos+8])[0]
		pos+=8
		self.rmc_mag_dec_dir = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos+=4
		self.rmc_mode_status = bytes[pos:pos+4].decode()
		pos+=4

		self.gsa_pdop = struct.unpack('<d', bytes[pos:pos+8])[0]
		pos+=8
		self.gsa_hdop = struct.unpack('<d', bytes[pos:pos+8])[0]
		pos+=8

	def toByte(self):
		byte = self.stat.to_bytes(4, byteorder='little')
		byte += struct.pack('<d', self.gga_time)
		byte += struct.pack('<d', self.gga_latitude)
		byte += self.gga_latitude_sign.to_bytes(4, byteorder='little')
		byte += struct.pack('<d', self.gga_longitude)
		byte += self.gga_longitude_sign.to_bytes(4, byteorder='little')
		byte += self.gga_mode_status.to_bytes(4, byteorder='little')
		byte += self.gga_satelite_num.to_bytes(4, byteorder='little')
		byte += struct.pack('<d', self.gga_hdop)
		byte += struct.pack('<d', self.gga_height_sea)
		byte += struct.pack('<d', self.gga_height_geoid)

		byte += struct.pack('<d', self.rmc_time)
		byte += struct.pack('<d', self.rmc_latitude)
		byte += self.rmc_latitude_sign.to_bytes(4, byteorder='little')
		byte += struct.pack('<d', self.rmc_longitude)
		byte += self.rmc_longitude_sign.to_bytes(4, byteorder='little')
		byte += struct.pack('<d', self.rmc_knots)
		byte += self.rmc_date.to_bytes(4, byteorder='little')
		byte += struct.pack('<d', self.rmc_azimuth)
		byte += struct.pack('<d', self.rmc_mag_dec)
		byte += self.rmc_mag_dec_dir.to_bytes(4, byteorder='little')
		byte += self.rmc_mode_status.encode()
		
		byte += struct.pack('<d', self.gsa_pdop)
		byte += struct.pack('<d', self.gsa_hdop)
		return byte

class INS():
	Stat = 0
	ins_Stat = 0
	Roll = 0
	Pitch = 0
	Yaw = 0
	Accel_X = 0
	Accel_Y = 0
	Accel_Z = 0
	Angle_X = 0
	Angle_Y = 0
	Angle_Z = 0

	def fromByte(self, bytes):
		pos = 0
		#通信状況
		self.Stat = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos+=4

		#Stat
		self.ins_Stattat = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

		#Roll
		self.Roll = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

		#Pitch
		self.Pitch = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

		#Yaw
		self.Yaw = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

		#Accel_X
		self.Accel_X = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

		#Accel_Y
		self.Accel_Y = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

		#Accel_Z
		self.Accel_Z = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

		#Angle_X
		self.Angle_X = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

		#Angle_Y
		self.Angle_Y = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

		#Angle_Z
		self.Accel_Z = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

	def toByte(self):
		#stat
		byte = self.Stat.to_bytes(4, byteorder='little')

		#ins_stat
		byte += self.ins_Stat.to_bytes(4, byteorder='little')

		#Roll
		byte += self.Roll.to_bytes(4, byteorder='little')

		#Pitch
		byte += self.Pitch.to_bytes(4, byteorder='little')

		#Yaw
		byte += self.Yaw.to_bytes(4, byteorder='little')

		#Accel_X
		byte += self.Accel_X.to_bytes(4, byteorder='little')

		#Accel_Y
		byte += self.Accel_Y.to_bytes(4, byteorder='little')

		#Accel_Z
		byte += self.Accel_Z.to_bytes(4, byteorder='little')

		#Angle_X
		byte += self.Angle_X.to_bytes(4, byteorder='little')

		#Angle_Y
		byte += self.Angle_Y.to_bytes(4, byteorder='little')
		
		#Angle_Z
		byte += self.Angle_Z.to_bytes(4, byteorder='little')

		return byte

class IMU():
	Stat = 0
	imu_Stat = 0
	Roll = 0
	Pitch = 0
	Yaw = 0
	Accel_X = 0
	Accel_Y = 0
	Accel_Z = 0
	Angle_X = 0
	Angle_Y = 0
	Angle_Z = 0

	def fromByte(self, bytes):
		pos = 0
		#通信状況
		self.Stat = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos+=4

		#Stat
		self.imu_Stattat = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

		#Roll
		self.Roll = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

		#Pitch
		self.Pitch = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

		#Yaw
		self.Yaw = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

		#Accel_X
		self.Accel_X = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

		#Accel_Y
		self.Accel_Y = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

		#Accel_Z
		self.Accel_Z = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

		#Angle_X
		self.Angle_X = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

		#Angle_Y
		self.Angle_Y = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

		#Angle_Z
		self.Accel_Z = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

	def toByte(self):
		#stat
		byte = self.Stat.to_bytes(4, byteorder='little')

		#stat
		byte += self.imu_Stat.to_bytes(4, byteorder='little')

		#Roll
		byte += self.Roll.to_bytes(4, byteorder='little')

		#Pitch
		byte += self.Pitch.to_bytes(4, byteorder='little')

		#Yaw
		byte += self.Yaw.to_bytes(4, byteorder='little')

		#Accel_X
		byte += self.Accel_X.to_bytes(4, byteorder='little')

		#Accel_Y
		byte += self.Accel_Y.to_bytes(4, byteorder='little')

		print("byte = ", byte)

		#Accel_Z
		byte += self.Accel_Z.to_bytes(4, byteorder='little')

		#Angle_X
		byte += self.Angle_X.to_bytes(4, byteorder='little')

		#Angle_Y
		byte += self.Angle_Y.to_bytes(4, byteorder='little')
		
		#Angle_Z
		byte += self.Angle_Z.to_bytes(4, byteorder='little')

		return byte

class Altmt():
	Stat = 0
	Distance = 0
	Ratio = 0
	Status = 0

	def fromByte(self, bytes):
		pos = 0
		#通信状況
		self.Stat = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos+=4

		#距離
		self.Distance = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

		#電波強度
		self.Ratio = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

		#ステータス
		self.Status = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

	def toByte(self):
		#stat
		byte = self.Stat.to_bytes(4, byteorder='little')

		#距離
		byte += self.Distance.to_bytes(4, byteorder='little')

		#電波強度
		byte += self.Ratio.to_bytes(4, byteorder='little')

		#ステータス
		byte += self.Status.to_bytes(4, byteorder='little')

		return byte

class I2C_HMC():
	stat = 0
	Ax = 0
	Ay = 0
	Az = 0
	Mx = 0
	My = 0
	Mz = 0
	Head = 0
	Pitch = 0
	Roll = 0
	Temp = 0

	def fromByte(self, bytes):
		pos = 0
		#通信状況
		self.stat = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos+=4

		#加速度X
		self.Ax = int.from_bytes(bytes[pos:pos+4], byteorder='little', signed=False)
		pos += 4

		#加速度Y
		self.Ay = int.from_bytes(bytes[pos:pos+4], byteorder='little', signed=False)
		pos += 4

		#加速度Z
		self.Az = int.from_bytes(bytes[pos:pos+4], byteorder='little', signed=False)
		pos += 4

		#磁気X
		self.Mx = int.from_bytes(bytes[pos:pos+4], byteorder='little', signed=False)
		pos += 4

		#磁気Y
		self.My = int.from_bytes(bytes[pos:pos+4], byteorder='little', signed=False)
		pos += 4

		#磁気Z
		self.Mz = int.from_bytes(bytes[pos:pos+4], byteorder='little', signed=False)
		pos += 4

		#ヘッディング
		self.Head = int.from_bytes(bytes[pos:pos+4], byteorder='little', signed=False)
		pos += 4

		#ピッチ
		self.Pitch = int.from_bytes(bytes[pos:pos+4], byteorder='little', signed=False)
		pos += 4

		#ロール
		self.Roll = int.from_bytes(bytes[pos:pos+4], byteorder='little', signed=False)
		pos += 4

		#温度
		self.Temp = int.from_bytes(bytes[pos:pos+4], byteorder='little', signed=False)
		pos += 4

	def toByte(self):
		#stat
		byte = self.stat.to_bytes(4, byteorder='little')

		#加速度X
		byte += self.Ax.to_bytes(4, byteorder='little', signed=False)

		#加速度Y
		byte += self.Ay.to_bytes(4, byteorder='little', signed=False)

		#加速度Z
		byte += self.Az.to_bytes(4, byteorder='little', signed=False)

		#磁気X
		byte += self.Mx.to_bytes(4, byteorder='little', signed=False)

		#磁気Y
		byte += self.My.to_bytes(4, byteorder='little', signed=False)

		#磁気Z
		byte += self.Mz.to_bytes(4, byteorder='little', signed=False)

		#ヘッディング
		byte += self.Head.to_bytes(4, byteorder='little', signed=False)

		#ピッチ
		byte += self.Pitch.to_bytes(4, byteorder='little', signed=False)

		#ロール
		byte += self.Roll.to_bytes(4, byteorder='little', signed=False)

		#温度
		byte += self.Temp.to_bytes(4, byteorder='little', signed=False)

		return byte

class I2C_BME():
	stat = 0
	Press = 0
	Temp = 0
	Hum = 0
	GasRes = 0

	def fromByte(self, bytes):
		pos = 0
		#通信状況
		self.stat = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos+=4

		#気圧
		self.Press = int.from_bytes(bytes[pos:pos+4], byteorder='little', signed=False)
		pos += 4

		#気温
		self.Temp = int.from_bytes(bytes[pos:pos+4], byteorder='little', signed=True)
		pos += 4

		#湿度
		self.Hum = int.from_bytes(bytes[pos:pos+4], byteorder='little', signed=False)
		pos += 4

		#GasRes
		self.GasRes = int.from_bytes(bytes[pos:pos+4], byteorder='little', signed=False)
		pos += 4

	def toByte(self):
		#stat
		byte = self.stat.to_bytes(4, byteorder='little')

		#気圧
		byte += self.Press.to_bytes(4, byteorder='little', signed=False)

		#気温
		byte += self.Temp.to_bytes(4, byteorder='little', signed=True)

		#湿度
		byte += self.Hum.to_bytes(4, byteorder='little', signed=False)

		#GasRes
		byte += self.GasRes.to_bytes(4, byteorder='little', signed=False)

		return byte

class Ping():
	Stat = 0
	def fromByte(self, bytes):
		pos = 0
		self.Stat = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos += 4

	def toByte(self):
		byte = self.Stat.to_bytes(4, byteorder='little')
		return byte

class Camera():
	Stat = 0
	timestamp = 0
	img_data = ''

	def fromByte(self,bytes):
		pos = 0
		#
		self.Stat = int.from_bytes(bytes[pos:pos+4], byteorder='little', signed=False)
		pos += 8
		#タイムスタンプ
		self.timestamp = int.from_bytes(bytes[pos:pos+8], byteorder='little', signed=False)
		pos += 8
		#画像データ
		self.img_data = bytes[pos:pos+16588800]

class Cube_Recv():
	Stat = 0
	time_unix_usec = 0
	time_boot_ms = 0

	def fromByte(self, bytes):
		pos = 0
		
		self.Stat = int.from_bytes(bytes[pos:pos+4], byteorder='little')
		pos+=8
		self.time_unix_usec = struct.unpack('<Q', bytes[pos:pos+8])[0]
		pos+=8
		self.time_boot_ms = struct.unpack('<I', bytes[pos:pos+8])[0]
		pos+=8

	def toByte(self):
		byte = self.stat.to_bytes(4, byteorder='little')
		byte += struct.pack('<Q', self.time_unix_usec)
		byte += struct.pack('<I', self.time_boot_ms)

		return byte

class Cube_Send():
	timestamp = 0.0
	vx = 0.0
	vy = 0.0
	vz = 0.0
	yaw_rate = 0.0

	def fromByte(self, bytes):
		pos = 0
		
		self.timestamp = struct.unpack('<f', bytes[pos:pos+4])[0]
		pos+=4
		self.vx = struct.unpack('<f', bytes[pos:pos+4])[0]
		pos+=4
		self.vy = struct.unpack('<f', bytes[pos:pos+4])[0]
		pos+=4
		self.vz = struct.unpack('<f', bytes[pos:pos+4])[0]
		pos+=4
		self.yaw_rate = struct.unpack('<f', bytes[pos:pos+4])[0]
		pos+=4

	def toByte(self):
		byte = struct.pack('<d', self.timestamp)
		byte += struct.pack('<f', self.vx)
		byte += struct.pack('<f', self.vy)
		byte += struct.pack('<f', self.vz)
		byte += struct.pack('<f', self.yaw_rate)

		return byte

class FailsafeInfo():
	errcode = []

	def fromByte(self, bytes):
		pos = 0
		for i in range(34):
			self.errcode.append(int.from_bytes(bytes[pos:pos+4], byteorder='little'))
			pos+=4

	def toByte(self):
		#死活情報
		for i in range(self.num):
			byte += self.errcode[i].to_bytes(4, byteorder='little')

		return byte

class Shmem:
	dictConf = {}
	shm = None					# 共有メモリ
	size = 0
	kind = ShmemKind.NONE
	current = ShmemKind.NONE
	mm = None					# アドレス

	def __init__(self, configfile):
		config = configparser.ConfigParser()
		config.read(configfile, encoding='utf-8')
		if len(config.sections()) == 0:
			message = 'section does not exist in ' + configfile
			syslog.syslog(message)
		
		for section in config.sections():
			self.dictConf[section] = config[section]

	def open(self, name, kind):
		try:
			self.size = int(self.dictConf[name]['size'])
		except:
			message = name + ' size does not exist in config file.'
			syslog.syslog(message)

			return False

		try:
			if (int(self.dictConf[name]['kind']) == 1):
				self.kind = ShmemKind.PLATFORM
			elif(int(self.dictConf[name]['kind']) == 2):
				self.kind = ShmemKind.USER
			else:
				self.kind = ShmemKind.NONE

			self.shm = ipc.SharedMemory(name, ipc.O_RDWR)
		except:
			message = name + ' kind does not exist in config file.'
			syslog.syslog(message)

			return False

		self.mm = mmap.mmap(self.shm.fd, self.shm.size)
		if self.mm is None:
			self.shm.close_fd()
			self.shm = None
			return False

		self.current = kind
		
		return True

	def close(self):
		if self.mm is not None:
			self.mm.close()
			self.mm = None

		if self.shm is not None:
			self.shm.close_fd()
			self.shm = None

	def read(self):
		if self.shm is None:
			message = (str)(self.shm) + ' is none'
			syslog.syslog(message)

			return None
		else:
			# 先頭にシーク
			self.mm.seek(0)
			# 共有メモリ読み込み
			#print(self.size)
			return self.mm.read(self.size)

	def write(self, bytes):
		if self.shm is None:
			message = self.shm + ' is none'
			syslog.syslog(message)

			return None
		else:
			if self.kind != self.current:
				message = 'kind is different. current = ' + self.current + '. kind = ' + self.kind
				syslog.syslog(message)

				print(self.kind, self.current)
				return None
			else:
				# 先頭にシーク
				self.mm.seek(0)
				# 共有メモリ書き込み
				return self.mm.write(bytes)
