import porto
import os
import multiprocessing
import types

NAME = os.path.basename(__file__)

def CT_NAME(suffix):
    global NAME
    return NAME + "-" + suffix

EPS = 0.95
TEST_LIM_SHARE = 0.75 #of the whole machine

DURATION = 1000 #ms
CPUNR = multiprocessing.cpu_count()
HAS_RT_LIMIT = os.access("/sys/fs/cgroup/cpu/cpu.rt_runtime_us", os.F_OK)
TEST_CORES_SHARE = CPUNR * TEST_LIM_SHARE

print "Available cores: {}, using EPS {}, run duration {} ms".format(CPUNR, EPS, DURATION)

def PrepareCpuContainer(ct, guarantee, limit, rt = False):
    if (rt):
        ct.SetProperty("cpu_policy", "rt")
    else:
        ct.SetProperty("cpu_policy", "normal")

    if (limit != 0.0):
        ct.SetProperty("cpu_limit", "{}c".format(limit))

    if (guarantee != 0.0):
        ct.SetProperty("cpu_guarantee", "{}c".format(guarantee))

    ct.SetProperty("cwd", os.getcwd())
    ct.SetProperty("command", "bash -c \'read; stress -c {} -t {}\'"\
                              .format(CPUNR, DURATION / 1000))
    ct.Limit = limit
    ct.Guarantee = guarantee
    ct.Eps = EPS

    #Adjust EPS for low limits
    if limit:
        ct.Eps = min(limit * 0.75, ct.Eps)
    elif guarantee:
        ct.Eps = min(guarantee * 0.75, ct.Eps)

    (ct.fd1, ct.fd2) = os.pipe()
    ct.SetProperty("stdin_path", "/dev/fd/{}".format(ct.fd1))

    ct.Start()

    return ct

def CheckCpuContainer(ct):
    assert ct.Wait(DURATION * 2) == ct.name, "container running time twice exceeded "\
                                             "expected duration {}".format(DURATION)

    usage = int(ct.GetProperty("cpuacct.usage")) / (10.0 ** 9) / (DURATION / 1000)

    print "{} : cpuacct usage: {}".format(ct.name, usage)

    assert ct.GetProperty("exit_status") == "0", "stress returned non-zero, stderr: {}"\
                                                .format(ct.GetProperty("stderr"))
    if ct.Limit != 0.0:
        assert usage < (ct.Limit + ct.Eps), "usage {} should be at most {}"\
                                          .format(usage, ct.Limit + ct.Eps)
    if ct.Guarantee != 0.0:
        assert usage > (ct.Guarantee - ct.Eps), "usage {} should be at least {}"\
                                              .format(usage, ct.Guarantee - ct.Eps)
    ct.Destroy()

    return usage

def KickCpuContainer(ct):
    os.close(ct.fd1)
    os.close(ct.fd2)
    return ct

def RunOneCpuContainer(ct):
    ct.Kick()
    ct.Check()
    return ct

def AllocContainer(conn, suffix):
    ct = conn.CreateWeakContainer(CT_NAME(suffix))
    ct.Prepare = types.MethodType(PrepareCpuContainer, ct)
    ct.Check = types.MethodType(CheckCpuContainer, ct)
    ct.Kick = types.MethodType(KickCpuContainer, ct)
    ct.RunOne = types.MethodType(RunOneCpuContainer, ct)
    return ct

def SplitLimit(conn, prefix, total, n, rt = False):
    conts = []
    for i in range(0, n):
        ct = conn.AllocContainer("{}_{}".format(prefix, i))
        conts += [ct.Prepare(0.0, total / n, rt)]

    for ct in conts:
        ct.Kick()

    for ct in conts:
        ct.Check()

conn = porto.Connection(timeout=30)
conn.AllocContainer = types.MethodType(AllocContainer, conn)

print "\nSet 1c limit for single container:"

conn.AllocContainer("normal_one_core").Prepare(0.0, 1.0).RunOne()

if CPUNR > 1:
    print "\nSet 1.5c limit for single container:"
    conn.AllocContainer("normal_one_and_half_core").Prepare(0.0, 1.5).RunOne()

if CPUNR > 2:
    print "\nSet {}c (CPUNR - 1) limit for single container:".format(CPUNR - 1)
    ct = conn.AllocContainer("normal_minus_one_core").Prepare(0.0, float(CPUNR) - 1.0).RunOne()

if HAS_RT_LIMIT:
    print "\nSet 1c limit for single rt container:"
    ct = conn.AllocContainer("rt_one_core").Prepare(0.0, 1.0, rt=True).RunOne()

    if CPUNR > 1:
        print "\nSet 1.5c limit for single rt container:"
        ct = conn.AllocContainer("rt_one_and_half_core").Prepare(0.0, 1.5, rt=True).RunOne()

    if CPUNR > 2:
        print "\nSet {}c (CPUNR - 1) limit for single rt container:".format(CPUNR - 1)
        ct = conn.AllocContainer("rt_minus_one_core").Prepare(0.0, float(CPUNR) - 1.0, rt=True)
        ct.RunOne()

if CPUNR > 1:
    print "\nSet {}c guarantee for 1 of 2 containers:".format(CPUNR * 2 / 3)
    ct1 = conn.AllocContainer("normal_half_0").Prepare(0.0, 0.0)
    ct2 = conn.AllocContainer("normal_half_guaranteed").Prepare(CPUNR * 2 / 3, 0.0)

    ct1.Kick(); ct2.Kick()
    ct1.Check(); ct2.Check()

    print "\nSplit {}c limit equally btwn 2 containers:".format(TEST_CORES_SHARE)
    SplitLimit(conn, "normal_half", TEST_CORES_SHARE, 2, rt = False)

    if HAS_RT_LIMIT:
        print "\nSplit {}c limit equally btwn 2 rt containers:"\
              .format(TEST_CORES_SHARE)
        SplitLimit(conn, "rt_half", TEST_CORES_SHARE, 2, rt = True)

if CPUNR > 2:
    print "\nSet {}c guarantee for 1 of 3 containers:".format(CPUNR / 2)

    conts = []
    conts += [conn.AllocContainer("normal_third_1").Prepare(0.0, 0.0)]
    conts += [conn.AllocContainer("normal_third_2").Prepare(0.0, 0.0)]
    conts += [conn.AllocContainer("normal_third_guaranteed").Prepare(CPUNR / 2, 0.0)]

    for ct in conts:
        ct.Kick()

    for ct in conts:
        ct.Check()

    print "\nSplit {}c limit equally btwn 3 containers:".format(TEST_CORES_SHARE)
    SplitLimit(conn, "normal_third", TEST_CORES_SHARE, 3, rt = False)

    if HAS_RT_LIMIT:
        print "\nSplit {}c limit equally btwn 3 rt containers:"\
              .format(TEST_CORES_SHARE)
        SplitLimit(conn, "rt_third", TEST_CORES_SHARE, 3, rt = True)

    print "\nSet 0.3c limit for 3 containers:"
    conts = []
    for i in range(0, 3):
        conts += [conn.AllocContainer("one_third_{}".format(i)).Prepare(0.0, 0.33)]

    for ct in conts:
        ct.Kick()

    for ct in conts:
        ct.Check()

if CPUNR > 3:
    print "\nSet {}c guarantee for 1 of 4 containers:".format(CPUNR / 2)

    conts = []
    conts += [conn.AllocContainer("normal_quarter_0").Prepare(0.0, 0.0)]
    conts += [conn.AllocContainer("normal_quarter_1").Prepare(0.0, 0.0)]
    conts += [conn.AllocContainer("normal_quarter_2").Prepare(0.0, 0.0)]
    conts += [conn.AllocContainer("normal_quarter_guaranteed").Prepare(CPUNR / 2, 0.0)]

    for ct in conts:
        ct.Kick()

    for ct in conts:
        ct.Check()

    print "\nSplit {}c limit equally btwn 4 containers:".format(TEST_CORES_SHARE)
    SplitLimit(conn, "normal_quarter", TEST_CORES_SHARE, 4, rt = False)

    if HAS_RT_LIMIT:
        print "\nSplit {}c limit equally btwn 4 rt containers:"\
              .format(TEST_CORES_SHARE)
        SplitLimit(conn, "rt_quarter", TEST_CORES_SHARE, 4, rt = True)

    if CPUNR > 3 and HAS_RT_LIMIT:
        print "\nSet 1c guarantee for 3 containers and 1c limit for rt container:"
        conts = []
        for i in range(0, 3):
            conts += [conn.AllocContainer("one_third_{}".format(i)).Prepare(1.0, 0.0)]

        conts += [conn.AllocContainer("rt_guy").Prepare(0.0, 1.0, rt=True)]

        for ct in conts:
            ct.Kick()

        for ct in conts:
            ct.Check()

    if CPUNR > 7 and HAS_RT_LIMIT:
        print "\nSet 1c guarantee and 1.5c limit for 3 containers and 1c limit for 2 rt containers:"
        conts = []

        for i in range(0, 3):
            conts += [conn.AllocContainer("one_third_{}".format(i)).Prepare(1.0, 1.5)]

        for i in range(0, 2):
            conts += [conn.AllocContainer("rt_guy_{}".format(i))\
                                          .Prepare(0.0, 1.0, rt = True)]
        for ct in conts:
            ct.Kick()

        for ct in conts:
            ct.Check()
