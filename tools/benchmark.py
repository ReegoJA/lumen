"""Measure linear image error with independent high-sample references."""

import argparse
import csv
import hashlib
import json
import platform
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('renders/benchmark'))
    parser.add_argument('--backend', choices=['cpu', 'cuda'], default='cpu')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    records = []
    binary = args.binary.resolve()

    def render(scene, prefix, samples, seed, reference=None, traversal='bvh'):
        command = [str(binary), '--scene', scene, '--width', '96', '--height', '96',
                   '--spp', str(samples), '--seed', str(seed), '--threads', '4',
                   '--backend', args.backend, '--output', str(prefix),
                   '--traversal', traversal]
        if reference:
            command += ['--reference', str(reference)]
        subprocess.run(command, check=True)
        with prefix.with_suffix('.csv').open(newline='') as stream:
            records.extend(csv.DictReader(stream))

    for scene in ('cornell', 'glass', 'spheres'):
        reference = args.output / (scene + '-reference')
        render(scene, reference, 1024, 7001)
        for samples in (8, 32, 128):
            for seed in (1, 2, 3):
                render(scene, args.output / f'{scene}-{samples}-{seed}', samples, seed,
                       reference.with_suffix('.pfm'))
    render('spheres', args.output / 'spheres-brute', 8, 1,
           args.output / 'spheres-reference.pfm', 'brute')
    with (args.output / 'measurements.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)
    metadata = dict(platform=platform.platform(), processor=platform.processor(),
                    binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                    width=96, height=96, threads=4, reference_spp=1024,
                    reference_seed=7001, seeds=[1, 2, 3],
                    timing='Renderer wall time including allocations, excluding file output and BVH build',
                    memory='Explicit image, scene, and backend buffer byte estimate, not peak resident memory',
                    error='Mean squared linear RGB difference from a finite independent reference')
    (args.output / 'environment.json').write_text(json.dumps(metadata, indent=2) + '\n')


if __name__ == '__main__':
    main()
