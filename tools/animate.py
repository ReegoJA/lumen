"""Render a moving ceiling light and assemble a lossless WebP loop."""

import argparse
import math
from pathlib import Path
import subprocess

from PIL import Image


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', required=True)
    parser.add_argument('--output', default='renders/light-motion')
    parser.add_argument('--frames', type=int, default=24)
    args = parser.parse_args()
    if not 4 <= args.frames <= 120:
        parser.error('Use between 4 and 120 frames')
    folder = Path(args.output)
    folder.mkdir(parents=True, exist_ok=True)
    frames = []
    for index in range(args.frames):
        offset = .55 * math.sin(2 * math.pi * index / args.frames)
        prefix = folder / f'frame-{index:03d}'
        subprocess.run([args.binary, '--scene', 'cornell', '--width', '256', '--height', '256',
                        '--spp', '64', '--threads', '4', '--seed', '1', '--light-x', str(offset),
                        '--output', str(prefix)], check=True)
        with Image.open(prefix.with_suffix('.ppm')) as image:
            frames.append(image.copy())
    frames[0].save(folder/'light-motion.webp', save_all=True, append_images=frames[1:],
                   duration=140, loop=0, lossless=True)
    frames[0].save(folder/'light-still.png')


if __name__ == '__main__':
    main()
