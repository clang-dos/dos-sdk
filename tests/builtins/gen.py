# Generates vec.h: inputs and expected results for rtt.c, computed on the
# host. Needs numpy. Run from this directory: python3 gen.py
# The tables are kept small enough to fit in a .COM program with the code.
import random, struct, math
random.seed(7)
M32=(1<<32)-1; M64=(1<<64)-1
def s32(x): x&=M32; return x-(1<<32) if x>>31 else x
def s64(x): x&=M64; return x-(1<<64) if x>>63 else x
def tdiv(a,b): q=abs(a)//abs(b); return q if (a<0)==(b<0) else -q
def tmod(a,b): return a-b*tdiv(a,b)
def f32(x): return struct.unpack('<I',struct.pack('<f',x))[0]
def tof(b): return struct.unpack('<f',struct.pack('<I',b))[0]
def f64(x): return struct.unpack('<Q',struct.pack('<d',x))[0]
def tod(b): return struct.unpack('<d',struct.pack('<Q',b))[0]
def r32(): return random.choice([0,1,-1,0x7fffffff,-0x80000000,random.randint(-100,100),random.randint(-(1<<31),(1<<31)-1),random.randint(0,0xffff)])
def r64(): return random.choice([0,1,-1,(1<<63)-1,-(1<<63),random.randint(-1000,1000),random.randint(-(1<<63),(1<<63)-1),random.randint(-(1<<33),1<<33)])
def rf(): return random.choice([0.0,-0.0,1.0,-2.5,3.0e38,1e-38,1e-45,random.uniform(-1e6,1e6),random.uniform(-1,1),float(random.randint(-1<<24,1<<24))])
lines=[]
def emit(name,ctype,rows):
    lines.append(f"static const {ctype} {name}[][{len(rows[0])}] = {{")
    for r in rows: lines.append("  {"+", ".join(r)+"},")
    lines.append("};")
L=lambda v: f"{v & M32:#x}UL"
LL=lambda v: f"{v & M64:#x}ULL"
rows=[]
for _ in range(60):
    a,b=r32(),r32()
    if b==0: b=7
    if a==-(1<<31) and b==-1: b=3
    rows.append([L(a),L(b),L(tdiv(a,b)),L(tmod(a,b)),L((a&M32)//(b&M32)),L((a&M32)%(b&M32)),L(a*b)])
emit("d32","unsigned long",rows)
rows=[]
for _ in range(60):
    a,b=r64(),r64()
    if b==0: b=7
    if a==-(1<<63) and b==-1: b=3
    sh=random.randint(0,63)
    rows.append([LL(a),LL(b),LL(tdiv(a,b)),LL(tmod(a,b)),LL((a&M64)//(b&M64)),LL((a&M64)%(b&M64)),LL(a*b),LL(sh),LL(a<<sh),LL((a&M64)>>sh),LL(a>>sh)])
emit("d64","unsigned long long",rows)
import numpy as np
rows=[]
for _ in range(75):
    a,b=np.float32(rf()),np.float32(rf())
    with np.errstate(all='ignore'):
        ops=[a+b,a-b,a*b,(a/b if b!=0 else np.float32(0))]
    rows.append([L(f32(float(a))),L(f32(float(b)))]+[L(int(np.array(o,dtype=np.float32).view(np.uint32))) for o in ops]+[L(1 if a<b else 0), L(s32(int(a)) if abs(float(a))<2**31 else 0)])
emit("ff","unsigned long",rows)
rows=[]
for _ in range(75):
    a,b=rf()*random.choice([1,1e100,1e-100]),rf()
    ops=[a+b,a-b,a*b,(a/b if b!=0 else 0.0)]
    i64 = int(a) if abs(a)<2**63 else 0
    rows.append([LL(f64(a)),LL(f64(b))]+[LL(f64(o)) for o in ops]+[LL(1 if a<b else 0),LL(i64),LL(f64(float(np.float32(a)) if abs(a)<3e38 else 0.0))])
emit("dd","unsigned long long",rows)
open("vec.h","w").write("\n".join(lines)+"\n")
