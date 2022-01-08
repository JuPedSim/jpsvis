import sys
from pathlib import Path
import argparse
from typing import Tuple
import xml.etree.ElementTree as ET
import numpy as np

parser = argparse.ArgumentParser(
    description='''Modify trajectory-files to be
    visualized with jpsvis.
    Trajectories are converted from cm to m.
    Moreover, new columns are added (A, B, ANGLE, COLOR)
    A and B are constants. ANGLE is zero (so agents are circles)
    COLOR is calculated based on the speed assuming
    a maximal speed of 1.5m/s.
    '''
)
parser.add_argument("-f", "--file", required=True, type=str,
                    help='Petrack trajectory file')
parser.add_argument("-d", "--df", default="10", dest='df',
                    help='''number of frames forward
                    to calculate the speed (default: 10)''')
args = parser.parse_args()


def extract_info(FileD) -> Tuple[str, int]:
    """extract first 3 lines from experiment file.
    Then append a description, a geometry and finally the
    jpscore header.
    # ---------------------------------------
    Assumptions being made:
    1. Header contains a line with PeTrack
    2. Header contains a line with "x/unit.
    # ---------------------------------------
    :param FileD: file
    :returns: header,
              fps (default: 16)
              and unit (default: cm)
    """
    header = f"description: trajectories converted by {sys.argv[0]}\n"
    fps = 16
    unit = "cm"
    hasHeader = False
    for line in FileD:
        if "PeTrack" in line:
            hasHeader = True

        if hasHeader:
            header += line.split("#")[-1].lstrip().split("fps")[0]
            if "x/" in line:
                unit = line.split("x/")[-1].split()[0]

            if "framerate" in line:
                header += "\ngeometry: geometry.xml\n"
                fps = line.split("fps")[0].split()[-1]

            if not line.startswith("#"):  # now the trajectories start
                break

    header += "ID\tFR\tX\tY\tZ\tA\tB\tANGLE\tCOLOR"
    try:
        fps = int(fps)
    except ValueError:
        sys.exit("can not extract fps")

    return (header, fps, unit)


def Speed(traj, df, fps) -> np.array:
    """Calculates the speed from the trajectory points.
    Using the forward formula
    speed(f) = (X(f+df) - X(f))/df [1]
    note: The last df frames are not calculated using [1].
    It is assumes that the speed in the last frames
    does not change
    :param traj: trajectory of ped (x, y). 2D array
    :param df: number of frames forwards
    :param fps: frames per seconds

    example:
    df=4, S=10
         0 1 2 3 4 5 6 7 8 9
       X * * * * * * * * * *
       V + + + + + +
         *       *
           *       *      X[df:]
    X[:S-df] *       *       │
    │          *       *   ◄─┘
    └────────►   *       *
                   *       *
    """
    Size = traj.shape[0]
    assert Size >= df, f"Trajectory too small. Size = {Size}, df= {df}"
    Speed = np.ones(Size)
    Delta_square = np.square(traj[df:, :] - traj[:Size-df, :])
    s = np.sqrt(Delta_square[:, 0] + Delta_square[:, 1])
    Speed[: Size-df] = s / df * fps
    Speed[Size-df:] = Speed[Size-df-1]
    return Speed


def write_geometry(data, Unit, geo_file):
    """Creates a bounding box around the trajectories

    :param data: 2D-array
    :param Unit: Unit of the trajectories (cm or m)
    :param geo_file: write geometry in this file
    :returns: geometry file names geometry.xml

    """
    Delta = 100 if Unit == "cm" else 1
    # 1 m around to better contain the trajectories
    xmin = (np.min(data[:, 2]) - Delta)
    xmax = (np.max(data[:, 2]) + Delta)
    ymin = (np.min(data[:, 3]) - Delta)
    ymax = (np.max(data[:, 3]) + Delta)
    data = ET.Element('geometry')
    data.set('version', '0.8')
    data.set('caption', 'experiment')
    data.set('unit', 'm')  # jpsvis does not support another unit!
    rooms = ET.SubElement(data, 'rooms')
    room = ET.SubElement(rooms, 'room')
    room.set('id', '0')
    room.set('caption', 'room')
    subroom = ET.SubElement(room, 'subroom')
    subroom.set('id', '0')
    subroom.set('caption', 'subroom')
    subroom.set('class', 'subroom')
    subroom.set('A_x', '0')
    subroom.set('B_y', '0')
    subroom.set('C_z', '0')
    # poly1
    polygon = ET.SubElement(subroom, 'polygon')
    polygon.set('caption', 'wall')
    polygon.set('type', 'internal')
    vertex = ET.SubElement(polygon, 'vertex')
    vertex.set('px', f'{xmin}')
    vertex.set('py', f'{ymin}')
    vertex = ET.SubElement(polygon, 'vertex')
    vertex.set('px', f'{xmax}')
    vertex.set('py', f'{ymin}')
    # poly2
    polygon = ET.SubElement(subroom, 'polygon')
    vertex = ET.SubElement(polygon, 'vertex')
    vertex.set('px', f'{xmax}')
    vertex.set('py', f'{ymin}')
    vertex = ET.SubElement(polygon, 'vertex')
    vertex.set('px', f'{xmax}')
    vertex.set('py', f'{ymax}')
    # poly3
    polygon = ET.SubElement(subroom, 'polygon')
    vertex = ET.SubElement(polygon, 'vertex')
    vertex.set('px', f'{xmax}')
    vertex.set('py', f'{ymax}')
    vertex = ET.SubElement(polygon, 'vertex')
    vertex.set('px', f'{xmin}')
    vertex.set('py', f'{ymax}')
    # poly4
    polygon = ET.SubElement(subroom, 'polygon')
    vertex = ET.SubElement(polygon, 'vertex')
    vertex.set('px', f'{xmin}')
    vertex.set('py', f'{ymax}')
    vertex = ET.SubElement(polygon, 'vertex')
    vertex.set('px', f'{xmin}')
    vertex.set('py', f'{ymin}')
    b_xml = ET.tostring(data, encoding='utf8', method='xml')
    with open(geo_file, "wb") as f:
        f.write(b_xml)


def data_at_frame(data, frame) -> np.array:
    """Get data at frame
    :param data: the trajectories from the file
    :param frame: frame number
    :returns: data at frame <frame>
    """
    return data[data[:, 1] == frame]


if __name__ == '__main__':
    File = Path(args.file)
    if not File.exists():
        sys.exit(f'file {File} does not exist!')

    try:
        df = int(args.df)
    except ValueError:
        sys.exit(f"can not convert input df {args.df} to int")

    with open(File, encoding="utf8") as f:
        header, fps, unit_s = extract_info(f)
        unit = 100 if unit_s == "cm" else 1
        print(f"file: {File}")
        print(header)
        print(f"fps: {fps}")
        print(f"df: {df}")
        print(f"unit: {unit_s}")
        data = np.loadtxt(File)
        rows, cols = data.shape
        A = 0.3 * np.ones((rows, 1)) * unit  # circle with radius 30cm
        B = 0.3 * np.ones((rows, 1)) * unit  # circle with radius 30cm
        ANGLE = np.zeros((rows, 1))  # does not matter since circles
        COLOR = 100 * np.ones((rows, 1))  # will be set wrt. speed
        data = np.hstack((data, A, B, ANGLE, COLOR))
        geometry_file = File.parent.joinpath("geometry.xml")
        write_geometry(data, unit, geometry_file)
        shape = data.shape
        result = np.empty(shape[1])
        frames = np.unique(data[:, 1]).astype(int)
        ids = np.unique(data[:, 0]).astype(int)
        v0 = 1.5 * unit  # max. speed (assumed) [unit/s]
        print("--> sort trajectories frame-wise")
        for frame in frames:
            data_fr = data_at_frame(data, frame)
            result = np.vstack((result, data_fr))

        print("--> calculate speeds")
        for i in ids:
            ped = data[data[:, 0] == i]
            speed = Speed(ped[:, 2:4], df, fps)
            result[result[:, 0] == i, -1] = speed/v0*255

        filename = File.parent.joinpath("jps_" + File.name)
        print(f"--> write results in {filename}")
        np.savetxt(filename, result[1:, :],
                   # skip the first line (initialization)
                   fmt=["%d", "%d",  # id frame
                        "%1.2f", "%1.2f", "%1.2f",  # x, y, z
                        "%1.2f", "%1.2f",  # A, B
                        "%1.2f",  # Angle
                        "%d"],  # Color
                   header=header,
                   comments='#',
                   delimiter='\t')
