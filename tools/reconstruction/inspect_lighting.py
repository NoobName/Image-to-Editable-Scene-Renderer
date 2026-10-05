"""Regenerate lighting diagnostic PNGs from validated float evidence; no refit or model inference."""
import argparse
from pathlib import Path
import numpy as np
from lighting_contract import KEYS
from package_schema import read_json
from runtime_dds import read_dds
from scene_package import load_package
from pipeline.lighting_backend import LightingEstimate
from pipeline.lighting_assets import write_lighting_diagnostic


def inspect(package):
    root = Path(package).resolve(strict=True); load_package(root)
    data = read_json(root/'lighting/lighting.json')
    arrays = [read_dds(root/data['maps'][key]['path'])[1] for key in KEYS]
    with np.load(root/'lighting/fit_evidence.npz', allow_pickle=False) as evidence:
        estimate = LightingEstimate(data['sourceLighting'], data['fit'], arrays[0][..., :3], arrays[1][..., :3],
                                    arrays[2], arrays[3], evidence['weights'], evidence['rejection_flags'])
    write_lighting_diagnostic(root/'lighting', estimate)
    print(root/'lighting/diagnostic.png')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument('packages', type=Path, nargs='+')
    for package in parser.parse_args().packages: inspect(package)
