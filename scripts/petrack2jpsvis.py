import sys
from pathlib import Path
import argparse
import xml.etree.ElementTree as ET
import logging as log
import numpy as np
from pandas import read_csv


log.basicConfig(level=log.DEBUG,
                format='%(levelname)s : %(message)s')


def check_positive_int(value):
    ivalue = int(value)
    if ivalue <= 0:
        raise argparse.ArgumentTypeError(
            f"{value} is an invalid value. Positive value required.")
    return ivalue


parser = argparse.ArgumentParser(
    description='''Modify trajectory-files to be
    visualized with jpsvis.
    New columns 'A', 'B' representing ellipses major axis (Agent Ellipsis),
    'ANGLE' representing the Agent's orientation and
    'COLOR' representing the Agent's speed, are added.
    'A' and 'B' are constants.
    'COLOR' and 'ANGLE' are calculated based on the speed assuming
    a maximal speed of v0=1.5m/s (color = v/v0*255).
    The name of the output file is the name of the input file
    prefixed with 'jps_' and ends with '.txt'
    '''
)
parser.add_argument('file', type=str,
                    help='Petrack trajectory file')
parser.add_argument("-u", "--unit", type=str,
                    choices=["m", "cm"],
                    help="""Specify the length unit used to represent agent
                    positions in the input PedTrack file.
                    Note: This argument information  maybe important when
                    passing PeTrack-files without header.""")

parser.add_argument("-d", "--df", default="10", dest='df',
                    type=check_positive_int,
                    help='''number of frames forward
                    to calculate the speed (default: 10)''')
args = parser.parse_args()


def extract_info(file_obj):
    """Extract 'unit' and 'fps' from Petrack file.

    Then append a description, a geometry and finally the
    jpscore header with the fps information.
    Keep the original header, if existing.
    # ---------------------------------------
    Assumptions being made:
    1. Header contains a line with the words 'PeTrack' or '<number>'
    2. Header contains a line with "x/unit or <x> [in unit]
    3. Header contains a line with fps information as follows:
       # framerate: N fps
    # ---------------------------------------
    :param file_obj: file object
    :returns: header,
              fps (default: 16)
              and unit (default: cm)

    """
    header = f"description: trajectories converted by {sys.argv[0]}\n"
    fps = 16
    unit = "cm"
    has_petrack_header = False
    has_fps = False
    has_unit = False
    for line in file_obj:
        if "PeTrack" in line or "<number>" in line:
            has_petrack_header = True

        if has_petrack_header:
            if not line.startswith("#"):  # now the trajectories start
                break

            header += line
            if "x/" in line:
                unit = line.split("x/")[-1].split()[0]
                has_unit = True

            if "<x>" in line:
                # <number> <frame> <x> [in m] <y> [in m] <z> [in m]
                unit = line.split("<x>")[-1].split()[1].strip("]")
                has_unit = True

            if "framerate" in line:
                fps = line.split("fps")[0].split()[-1]
                has_fps = True
    # jpsvis part
    header += f"\nframerate: {fps}"
    header += "\ngeometry: geometry.xml\n"
    header += "ID\tFR\tX\tY\tZ\tA\tB\tANGLE\tCOLOR"
    if not has_petrack_header:
        log.warning("Trajectory file does not have header.")

    if not has_fps:
        log.warning("did not find fps in header. Assuming fps=16")

    if not has_unit:  # did not find unit in header
        if args.unit is not None:
            log.info(f"Unit passed: {args.unit}")
            unit = args.unit
        else:
            log.warning("did not find unit in header. Assuming unit=cm")
    else:  # found unit in header
        if args.unit is not None and args.unit != unit:
            log.warning(f"""Unit passed: {args.unit}
            but unit detected in header {unit}.
            Using: {unit}""")

    try:
        fps = int(fps)
    except ValueError:
        log.error(
            f"fps <{fps}> in header can not be converted to int")

        sys.exit()

    return (header, fps, unit)


def speed_angle(traj, df, fps):
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
    Speed = np.ones(Size)
    Angle = np.zeros(Size)
    if Size < df:
        log.warning(
            f"""The number of frames used to calculate the speed {df}
            exceeds the total amount of frames ({Size}) in this trajectory.""")
        return (Speed, Angle)

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
    return (Speed, Angle)


def write_geometry(data, Unit, geo_file):
    """Creates a bounding box around the trajectories

    :param data: 2D-array
    :param Unit: Unit of the trajectories (cm or m)
    :param geo_file: write geometry in this file
    :returns: geometry file names geometry.xml

    """
    Delta = 100 if Unit == "cm" else 1
    # 1 m around to better contain the trajectories
    xmin = np.min(data[:, 2]) - Delta
    xmax = np.max(data[:, 2]) + Delta
    ymin = np.min(data[:, 3]) - Delta
    ymax = np.max(data[:, 3]) + Delta
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


def extend_data(data, _unit):
    """Append some columns to the trajectories

    For visualisation purposes. These columns are:
    - height: if missing in the trajectory file
    - A: Semi-axis of the agent
    - B: Semi-axis of the agent.
    - ANGLE: angle of the agent.
    - COLOR: Color of the agent.
    :param data: input trajectories
    :type data: np.array
    :returns: np.array

    :param data:
    :param _unit:

    """
    rows, cols = data.shape
    H = 1.5 * np.ones((rows, 1)) * _unit  # height
    A = 0.3 * np.ones((rows, 1)) * _unit  # semi-axis of ellipse
    B = 0.2 * np.ones((rows, 1)) * _unit  # semi-axis of ellipse
    ANGLE = np.zeros((rows, 1))  # angle in degree
    COLOR = 100 * np.ones((rows, 1))  # will be set wrt. speed
    if cols == 4:  # some trajectories do not have Z
        data = np.hstack((data, H, A, B, ANGLE, COLOR))
    else:
        data = np.hstack((data, A, B, ANGLE, COLOR))

    return data


def write_trajectories(result, header, file_path):
    """write the resulting trajectories in a file

    :param result: trajectories
    :type result: np.array
    :param header: Header
    :type header: str
    :param file_path: input file
    :type file_path: Path
    :returns: the name of the file is jps_Filename

    """
    filename = file_path.parent.joinpath("jps_" + file_path.stem + ".txt")
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
    size_mb = Path(filename).stat().st_size/1024/1024
    log.info(
        f"output: {filename} ({size_mb:,.2f} MB)")


def write_debug_msg(traj_file, fps, df, unit_s):
    size_mb = traj_file.stat().st_size/1024/1024
    log.info(f"input : {traj_file} ({size_mb:,.2f} MB)")
    log.info(f"fps   : {fps}")
    log.info(f"df    : {df}")
    log.info(f"unit  : {unit_s}")


def main():
    input_file = Path(args.file)
    df = args.df
    try:
        with open(input_file, encoding="utf8") as finput:
            header, fps, unit_s = extract_info(finput)
            unit = 100 if unit_s == "cm" else 1
            v0 = 1.5 * unit  # max. speed (assumed) [unit/s]
            write_debug_msg(input_file, fps, df, unit_s)
            data = read_csv(input_file,
                            sep=r"\s+",
                            dtype=np.float64,
                            comment="#").values
            columns = data.shape[1]
            log.info(f"Got {columns} columns")
            nframes = np.unique(data[:, 1]).size
            if columns > 5:
                # keep the following cols: id fr x y z
                data = data[:, :5]

            data = data[data[:, 1].argsort()]  # sort data by frame
            data = extend_data(data, unit)
            geometry_file = input_file.parent.joinpath("geometry.xml")
            write_geometry(data, unit_s, geometry_file)
            agents = np.unique(data[:, 0]).astype(int)
            log.info(
                f"Got {agents.size} pedestrians and {nframes} frames.")
            for agent in agents:
                ped = data[data[:, 0] == agent]
                speed, angle = speed_angle(ped[:, 2:4], df, fps)
                data[data[:, 0] == agent, -1] = speed/v0*255
                data[data[:, 0] == agent, -2] = angle

            write_trajectories(data, header, input_file)

    except OSError as err:
        log.error(f"""trying to open file
        {err.filename}: {err. strerror}""")


if __name__ == '__main__':
    main()
