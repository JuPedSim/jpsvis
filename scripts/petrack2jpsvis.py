import sys
from pathlib import Path
import argparse
from typing import Tuple
import xml.etree.ElementTree as ET
import numpy as np
from pandas import read_csv

parser = argparse.ArgumentParser(
    description='''Modify trajectory-files to be
    visualized with jpsvis.
    New columns are added (A, B, ANGLE, COLOR)
    A and B are constants. ANGLE is zero (so agents are circles)
    COLOR is calculated based on the speed assuming
    a maximal speed of 1.5m/s.
    '''
)
parser.add_argument("-f", "--file", required=True, type=str,
                    help='Petrack trajectory file')
parser.add_argument("-d", "--df", default="10", dest='df',
                    type=int,
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
    hasPetrackHeader = False
    for line in FileD:
        if "PeTrack" in line:
            hasPetrackHeader = True

        if hasPetrackHeader:
            if not line.startswith("#"):  # now the trajectories start
                break

            header += line
            if "x/" in line:
                unit = line.split("x/")[-1].split()[0]

            if "framerate" in line:
                fps = line.split("fps")[0].split()[-1]

    # jpsvis part
    header += f"\nframerate: {fps}"
    header += "\ngeometry: geometry.xml\n"
    header += "ID\tFR\tX\tY\tZ\tA\tB\tANGLE\tCOLOR"
    try:
        fps = int(fps)
    except ValueError:
        sys.exit("can not extract fps")

    return (header, fps, unit)


def Speed_Angle(traj, df, fps) -> Tuple[np.array, np.array]:
    """Calculates the speed and the angle from the trajectory points.
    Using the forward formula
    speed(f) = (X(f+df) - X(f))/df [1]
    note: The last df frames are not calculated using [1].
    It is assumes that the speed in the last frames
    does not change
    :param traj: trajectory of ped (x, y). 2D array
    :param df: number of frames forwards
    :param fps: frames per seconds

    :returns: speed, angle

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
    Angle = np.ones(Size)
    Delta = traj[df:, :] - traj[:Size-df, :]
    Delta_X = Delta[:, 0]
    Delta_Y = Delta[:, 1]
    Delta_square = np.square(Delta)
    Delta_X_square = Delta_square[:, 0]
    Delta_Y_square = Delta_square[:, 1]
    Angle[: Size-df] = np.arctan2(Delta_Y,
                                  Delta_X)*180/np.pi
    s = np.sqrt(Delta_X_square + Delta_Y_square)
    Speed[: Size-df] = s / df * fps
    Speed[Size-df:] = Speed[Size-df-1]
    Angle[Size-df:] = Angle[Size-df-1]
    return Speed, Angle


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


def extend_data(data, _unit) -> np.array:
    """Append some new columns to the trajectories
    for visualisation purposes. These are:
    - height: if missing in the trajectory file
    - A: Semi-axis of the agent
    - B: Semi-axis of the agent.
    - ANGLE: angle of the agent.
    - COLOR: Color of the agent.
    :param data: input trajectories
    :type data: np.array
    :returns: np.array

    """
    rows, cols = data.shape
    H = 1.5 * np.ones((rows, 1)) * _unit  # hight 150cm
    A = 0.3 * np.ones((rows, 1)) * _unit  # circle with radius 30cm
    B = 0.2 * np.ones((rows, 1)) * _unit  # circle with radius 30cm
    ANGLE = np.zeros((rows, 1))  # does not matter since circles
    COLOR = 100 * np.ones((rows, 1))  # will be set wrt. speed
    if cols == 4:  # some trajectories do not have Z
        data = np.hstack((data, H, A, B, ANGLE, COLOR))
    else:
        data = np.hstack((data, A, B, ANGLE, COLOR))

    return data


def write_trajectories(result, header, File):
    """write the resulting trajecties in a file

    :param result: trajectories
    :type result: np.array
    :param header: Header
    :type header: str
    :param File: input file
    :type File: Path
    :returns: the name of the file is jps_Filename

    """
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


def write_debug_msg(File, fps, df, unit_s):
    print(f"file: {File}, Size: {File.stat().st_size/1024/1024:,.2f} MB")
    print(f"fps: {fps}")
    print(f"df: {df}")
    print(f"unit: {unit_s}")


def main():
    File = Path(args.file)
    if not File.exists():
        sys.exit(f'file {File} does not exist!')

    try:
        df = int(args.df)
    except ValueError:
        sys.exit(f"can not convert input df {args.df} to int")

    with open(File, encoding="utf8") as finput:
        header, fps, unit_s = extract_info(finput)
        unit = 100 if unit_s == "cm" else 1
        v0 = 1.5 * unit  # max. speed (assumed) [unit/s]
        write_debug_msg(File, fps, df, unit_s)
        data = read_csv(File,
                        sep=r"\s+",
                        dtype=np.float64,
                        comment="#").values
        data = data[data[:, 1].argsort()]  # sort by frame
        data = extend_data(data, unit)
        geometry_file = File.parent.joinpath("geometry.xml")
        write_geometry(data, unit, geometry_file)
        agents = np.unique(data[:, 0]).astype(int)
        for agent in agents:
            ped = data[data[:, 0] == agent]
            speed, angle = Speed_Angle(ped[:, 2:4], df, fps)
            data[data[:, 0] == agent, -1] = speed/v0*255
            data[data[:, 0] == agent, -2] = angle
        write_trajectories(data, header, File)


if __name__ == '__main__':
    # import cProfile
    # import pstats
    # profiler = cProfile.Profile()
    # profiler.enable()
    main()
    # profiler.disable()
    # stats = pstats.Stats(profiler).sort_stats('cumtime')
    # print(stats.print_stats())
