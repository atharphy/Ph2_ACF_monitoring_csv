##########################################################
# Program to copy the pixel configuraion from the input  #
# (*.txt) file to the output (*.txt) file of Ph2_ACF DAQ #
#by Mauro Dinardo #
##########################################################

from argparse import ArgumentParser


def ArgParser():
    parser = ArgumentParser()

    parser.add_argument('-f', '--fromFile', dest = 'fromFile', type = str, help = 'Take mask from this chip cfg file', required = True, default = '')
    parser.add_argument('-t', '--toFile',   dest = 'toFile',   type = str, help = 'Apply mask to this chip cfg file',  required = True, default = '')
    parser.add_argument('-o', '--outFile',  dest = 'outFile',  type = str, help = 'Write chip cfg to this file',       required = True, default = '')

    options = parser.parse_args()

    if options.fromFile:
        print('--> I\'m reading -from- file:', options.fromFile)
    if options.toFile:
        print('--> I\'m reading -to- file:', options.toFile)
    if options.outFile:
        print('--> I\'m reading output file:', options.outFile)

    return options


def readMaskFromFile(fileName):
    mask = []

    with open(fileName) as fin:
        lines     = fin.readlines()
        foundMask = False

        for it, line in enumerate(lines):
            if line.find('PIXELCONFIGURATION') != -1 or foundMask == True:
                if foundMask == False:
                    mask.append(lines[it-1])
                foundMask = True
                mask.append(line)

    return mask


def applyMaskToFile(fileName, mask):
    cfg = []

    with open(fileName) as fin:
        lines     = fin.readlines()
        foundMask = False

        for it, line in enumerate(lines):
            cfg.append(line)

            if line.find('PIXELCONFIGURATION') != -1:
                cfg.pop()
                cfg.pop()
                break

        cfg.extend(mask)

    return cfg


def saveCFGfie(fileName, cfg):
    with open(fileName, 'w', encoding='UTF8') as fout:
        for line in cfg:
            fout.write(line)


"""
#################
# Start program #
#################
"""

cmd      = ArgParser()
fromMask = readMaskFromFile(cmd.fromFile)
newCFG   = applyMaskToFile(cmd.toFile, fromMask)
saveCFGfie(cmd.outFile, newCFG)
