import sys
from os.path import exists
import argparse
import numpy as np
import xml.etree.ElementTree as ET

parser = argparse.ArgumentParser(
    description='''Modify trajectory-files to be
    visualized with jpsvis.
    Trajectories are converted from cm to m.
    Moreover, new columns are added (A, B, ANGLE, COLOR)
    '''
)
parser.add_argument("-f", "--file", required=True, type=str,
                    help='Petrack trajectory file')
parser.add_argument("--cm", dest='convert', action='store_false',
                    help='keep trajectories in cm')
parser.set_defaults(convert=True)
args = parser.parse_args()


def extract_info(F) -> (str, int):
    """
    extract first 3 lines from experiment file.
    Then append a description, a geometry and finally the
    jpscore header.
    return fps as well
    """
    header = "description: trajectories converted by {}\n".format(sys.argv[0])
    fps = 16
    for line in f:
        header += line.split("#")[-1].lstrip().split("fps")[0]
        if "framerate" in line:
            header += "\ngeometry: geometry.xml\n"
            fps = line.split("fps")[0].split()[-1]
            break

    header += "ID\tFR\tX\tY\tZ\tA\tB\tANGLE\tCOLOR"
    try:
        fps = int(fps)
    except ValueError:
        sys.exit("can not extract fps")

    return (header, fps)


def speed(data_now, data_future, df, fps) -> np.array:
    #pdb.set_trace()
    
    Pos2 = data_future[:, 2:4]
    Pos = data_now[:, 2:4]
    
    Delta_square = np.square(Pos2 - Pos)
    
    s = np.sqrt(Delta_square[:, 0] + Delta_square[:, 1])
    
    Speed = s/df*fps
    return Speed


def write_geometry(data, cm=1):
    '''
    Creates a bounding box around the trajectories
    Output: geometry.xml
    '''
    Delta = 5  # some more space to better contain the trajectories
    xmin = (np.min(data[2, :]) - Delta)/cm
    xmax = (np.max(data[2, :]) + Delta)/cm
    ymin = (np.min(data[3, :]) - Delta)/cm
    ymax = (np.max(data[3, :]) + Delta)/cm
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
    vertex.set('px', '%.2f' % xmin)
    vertex.set('py', '%.2f' % ymin)
    vertex = ET.SubElement(polygon, 'vertex')
    vertex.set('px', '%.2f' % xmax)
    vertex.set('py', '%.2f' % ymin)
    # poly2
    polygon = ET.SubElement(subroom, 'polygon')
    vertex = ET.SubElement(polygon, 'vertex')
    vertex.set('px', '%.2f' % xmax)
    vertex.set('py', '%.2f' % ymin)
    vertex = ET.SubElement(polygon, 'vertex')
    vertex.set('px', '%.2f' % xmax)
    vertex.set('py', '%.2f' % ymax)
    # poly3
    polygon = ET.SubElement(subroom, 'polygon')
    vertex = ET.SubElement(polygon, 'vertex')
    vertex.set('px', '%.2f' % xmax)
    vertex.set('py', '%.2f' % ymax)
    vertex = ET.SubElement(polygon, 'vertex')
    vertex.set('px', '%.2f' % xmin)
    vertex.set('py', '%.2f' % ymax)
    # poly4
    polygon = ET.SubElement(subroom, 'polygon')
    vertex = ET.SubElement(polygon, 'vertex')
    vertex.set('px', '%.2f' % xmin)
    vertex.set('py', '%.2f' % ymax)
    vertex = ET.SubElement(polygon, 'vertex')
    vertex.set('px', '%.2f' % xmin)
    vertex.set('py', '%.2f' % ymin)
    b_xml = ET.tostring(data, encoding='utf8', method='xml')
    with open("geometry.xml", "wb") as f:
        f.write(b_xml)


def data_at_frame(data, frame) -> np.array:
    return data[data[:, 1] == frame]


if __name__ == '__main__':
    File = args.file
    if not exists(File):
        sys.exit("file {} does not exist!".format(File))

    cm = 100 if args.convert else 1

    with open(File) as f:
        header, fps = extract_info(f)
        print("------")
        print("file: {}".format(File))
        print(header)
        print("fps: {}".format(fps))
        print("------")
        data = np.loadtxt(File)
        rows, cols = data.shape
        A = 30 * np.ones((rows, 1))
        B = 30 * np.ones((rows, 1))
        ANGLE = np.zeros((rows, 1))
        # COLOR = set_color(data)
        COLOR = 100 * np.ones((rows, 1))
        data = np.hstack((data, A, B, ANGLE, COLOR))
        write_geometry(data, cm)
        shape = data.shape
        result = np.empty(shape[1])
        cm2m = np.ones(shape[1])
        cm2m[2:-2] *= cm  # exclude id, fr, angle and color from conversion
        frames = np.unique(data[:, 1]).astype(int)
        df = 10
        v0 = 1.5
        
        for frame in frames:
            #print(frame, np.max(frames))
            data_fr = data_at_frame(data, frame)/cm2m
            data_fr2 = data_at_frame(data, frame+df)/cm2m
            #s = speed(data_fr, data_fr2, df, fps)
            #Color = s/v0*255            
            #data_fr[:, -1] = Color
            result = np.vstack((result, data_fr))

        filename = "jps_" + File
        np.savetxt(filename, result[1:, :],
                   fmt=["%d", "%d",  # id frame
                        "%1.2f", "%1.2f", "%1.2f",  # x, y, z
                        "%1.2f", "%1.2f",  # A, B
                        "%1.2f",  # Angle
                        "%d"],  # Color
                   header=header,
                   comments='#',
                   delimiter='\t')

        print("-> ", filename)
