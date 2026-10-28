#!/usr/bin/env python3
"""Run a WarpQuake2 timedemo on the Amiga and print its result.

  bench.py [--name NAME] [--no-upload] [--runs N] [--crc] [--prof] [--sample]
           [--demo demo1.dm2] [--mode 0] [--bpp 8|16] [--limit SECONDS] [-- extra engine arguments]

Uploads build/WarpQuake2 as WORK:Games/WarpQuake2/<NAME> (default WarpQuake2), so two
builds can sit side by side for A/B runs, then runs it:

  <NAME> -benchmark [-crc] [-prof] [-sample] +set basedir <BASEDIR> +set game warpq2
         +set fixedtime 100 +set sw_mode <MODE> +set vid_bpp <BPP> +set timedemo 1 +demomap <DEMO>

-benchmark makes the game quit when the demo ends, so the call returns at once, with the
"Q2BENCH ..." line it prints (and, with --prof, the phase table).  fixedtime 100 steps one
server frame per rendered frame, so every run draws the same frames and --crc compares
builds pixel for pixel.

The runner is a remote-shell command that runs one AmigaOS command line on the machine and
prints its output, named by AMIGA_RUNNER in the environment or in tools/ftp_config.local, e.g.
  AMIGA_RUNNER=env SOME_SETTING=... python3 -I /path/to/runner.py
It is called as: <runner> --cd <dir> --limit <s> --timeout <s> -- <command line>, where --limit
asks it to break the command after that many seconds.
AMIGA_Q2_BASEDIR (default Work:Games/Quake2) is where the game data is.

With --sample the profile is fetched to temp/ and summarised with tools/wprof.py against
build/WarpQuake2_sym.map (run "make prof" for it).
"""
import argparse, os, re, shlex, shutil, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
AMIGA_DIR = 'WORK:Games/WarpQuake2'


def config():
    cfg = {}
    path = os.path.join(HERE, 'ftp_config.local')
    if os.path.exists(path):
        for line in open(path):
            m = re.match(r'\s*(?:export\s+)?(AMIGA_\w+)\s*=\s*(.*?)\s*$', line)
            if m:
                cfg[m.group(1)] = m.group(2).strip('"\'')
    cfg.update((k, v) for k, v in os.environ.items() if k.startswith('AMIGA_'))
    return cfg


def run(runner, cmdline, limit):
    cmd = shlex.split(runner) + ['--cd', AMIGA_DIR, '--limit', str(limit), '--timeout', str(limit + 60), '--',
                                 cmdline]
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return p.stdout.decode('latin-1')


def put_config(basedir):
    """tools/bench_config.cfg as the game directory's config.cfg"""
    tmp = os.path.join(ROOT, 'temp', 'upload')
    os.makedirs(tmp, exist_ok=True)
    shutil.copyfile(os.path.join(HERE, 'bench_config.cfg'), os.path.join(tmp, 'config.cfg'))
    subprocess.run([sys.executable, os.path.join(HERE, 'amiput.py'), os.path.join(tmp, 'config.cfg'), '-d',
                    basedir + '/warpq2'], check=True, stdout=subprocess.DEVNULL)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--name', default='WarpQuake2')
    ap.add_argument('--binary', default=os.path.join(ROOT, 'build', 'WarpQuake2'))
    ap.add_argument('--no-upload', action='store_true')
    ap.add_argument('--runs', type=int, default=1)
    ap.add_argument('--crc', action='store_true')
    ap.add_argument('--prof', action='store_true')
    ap.add_argument('--sample', action='store_true')
    ap.add_argument('--demo', default='demo1.dm2')
    ap.add_argument('--mode', default='0')
    ap.add_argument('--bpp', default='8')
    ap.add_argument('--limit', type=int, default=400)
    ap.add_argument('--verbose', '-v', action='store_true')
    ap.add_argument('extra', nargs='*')
    a = ap.parse_args()

    cfg = config()
    runner = cfg.get('AMIGA_RUNNER')
    if not runner:
        sys.exit('bench: set AMIGA_RUNNER (environment or tools/ftp_config.local)')
    basedir = cfg.get('AMIGA_Q2_BASEDIR', 'Work:Games/Quake2')

    if not a.no_upload:
        src = a.binary
        if os.path.basename(src) != a.name:
            # amiput keeps the file name: a copy under the Amiga-side one
            os.makedirs(os.path.join(ROOT, 'temp', 'upload'), exist_ok=True)
            src = os.path.join(ROOT, 'temp', 'upload', a.name)
            shutil.copyfile(a.binary, src)
        subprocess.run([sys.executable, os.path.join(HERE, 'amiput.py'), src, '-d', AMIGA_DIR],
                       check=True, stdout=subprocess.DEVNULL)
        run(runner, 'protect %s/%s +e' % (AMIGA_DIR, a.name), 30)

    dash = ['-benchmark'] + (['-crc'] if a.crc else []) + (['-prof'] if a.prof else []) + \
           (['-sample'] if a.sample else [])
    # extra arguments up to the first '+' are options (-crcfile RAM:x), the rest commands
    first_plus = next((i for i, x in enumerate(a.extra) if x.startswith('+')), len(a.extra))
    dash += a.extra[:first_plus]
    plus = ['+set basedir %s +set game warpq2 +set fixedtime 100 +set sw_mode %s +set vid_bpp %s' % (basedir, a.mode, a.bpp)]
    plus += a.extra[first_plus:]
    plus += ['+set timedemo 1 +demomap %s' % a.demo]
    cmdline = '%s %s %s' % (a.name, ' '.join(dash), ' '.join(plus))

    results = []
    for i in range(a.runs):
        # from the defaults every time: archived cvars an earlier run set
        # (sw_subdiv16 0, say) would carry over in its config.cfg, and
        # without one "exec config.cfg" finds baseq2's
        put_config(basedir)
        out = run(runner, cmdline, a.limit)
        m = re.search(r'Q2BENCH frames=(\d+) ms=(\d+) ms_per_frame=([\d.]+) fps=([\d.]+) crc=([0-9a-f]+)', out)
        if a.verbose or not m:
            sys.stdout.write(out)
        elif a.prof:
            s = out.find('---- WarpQuake2 profile')
            e = out.find('-- timer:', s)
            sys.stdout.write(out[s:out.find('\n', e) + 1] if s >= 0 else '')
        if not m:
            sys.exit('bench: no Q2BENCH line (run %d)' % (i + 1))
        r = (int(m.group(1)), int(m.group(2)), float(m.group(3)), float(m.group(4)), m.group(5))
        results.append(r)
        print('%s run %d: %d frames, %.3f ms/frame, %.3f fps%s' % (a.name, i + 1, r[0], r[2], r[3],
                                                                    ', crc %s' % r[4] if a.crc else ''))
    if len(results) > 1:
        ms = [r[2] for r in results]
        print('%s: mean %.3f ms/frame (min %.3f, max %.3f), %.3f fps' % (a.name, sum(ms) / len(ms), min(ms), max(ms),
                                                                         1000.0 * len(ms) / sum(ms)))

    if a.sample:
        tmp = os.path.join(ROOT, 'temp')
        os.makedirs(tmp, exist_ok=True)
        subprocess.run([sys.executable, os.path.join(HERE, 'amiget.py'), 'RAM:WarpQuake2.wprf', '-o', tmp], check=True,
                       stdout=subprocess.DEVNULL)
        subprocess.run([sys.executable, os.path.join(HERE, 'wprof.py'), os.path.join(tmp, 'WarpQuake2.wprf'),
                        os.path.join(ROOT, 'build', 'WarpQuake2_sym.map'), os.path.join(tmp, 'profile.txt')],
                       check=True)
        print(open(os.path.join(tmp, 'profile.txt')).read())


if __name__ == '__main__':
    main()
