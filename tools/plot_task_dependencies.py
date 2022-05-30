#!/usr/bin/env python3

description = """
This file generates a graphviz file that represents the SWIFT tasks
dependencies.

Example: ./plot_task_dependencies.py dependency_graph.csv
"""
from pandas import read_csv
import numpy as np
from subprocess import call
import argparse
from os import path

# define task colours in the dict here:
task_colours = {
    "black_holes": "forestgreen",
    "stars": "darkorange1",
    "hydro": "blue3",
    "gravity": "red3",
    "RT": "springgreen",
    "sink": "lightseagreen",
}


def parse_args():
    """
    Parses command line arguments.

    Returns
    -------

    args: Namespace
        Namespace returned by argparse.ArgumentParser.parse_args()
        containing all arguments

    files:
        List of files parsed from the command line.

    Raises
    ------

    FileNotFoundError
        If any of the files provided on the command line doesn't exist
    """

    # description is string at the top of this file.
    parser = argparse.ArgumentParser(description=description)

    parser.add_argument(
        "-c",
        "--with-calls",
        dest="with_calls",
        help="Add the function calls of the tasks to the graph",
        action="store_true",
    )

    parser.add_argument(
        "-l",
        "--with-levels",
        dest="with_levels",
        help="Write the number of each task at each level for each task individually",
        action="store_true",
    )

    parser.add_argument(
        "files",
        nargs="+",
        type=str,
        help="Required file name(s) of .csv file(s) of the task dependencies "
        "generated by swift.",
    )

    args = parser.parse_args()
    files = args.files

    for f in files:
        if not path.exists(f):
            raise FileNotFoundError("You need to provide one file")

    if args.with_calls and args.with_levels:
        raise ValueError(
            "I can't run with --with-calls and",
            " --with-levels simultaneously. Pick one!",
        )

    return args, files


def get_git_version(f, git):
    """
    Read the git version from the file

    Parameters
    ----------

    f: str
        Filename

    git: str
        Git version of previous file

    Returns
    -------

    new_git: str
        Git version of current file
    """
    # read comment in csv file
    with open(f, "r") as f:
        line = f.readline()

    # check if really a comment
    if line[0] != "#":
        return None

    # remove trailing characters
    new_git = line[2:].rstrip()

    # check if previous and current are the same
    if git is not None and git != new_git:
        raise Exception("Files were not produced by the same version")

    return new_git


def get_task_colour(taskname):
    """
    Get the task colour based on its name.
    Default colour is black.


    Parameters
    ----------

    taskname: str
        name of the task at hand


    Returns
    -------

    colour: str
        colour string directly applicable in the dot file.
    """

    colour = "black"
    if task_is_black_holes(taskname):
        colour = task_colours["black_holes"]
    elif task_is_stars(taskname):
        colour = task_colours["stars"]
    elif task_is_hydro(taskname):
        colour = task_colours["hydro"]
    elif task_is_gravity(taskname):
        colour = task_colours["gravity"]
    elif task_is_RT(taskname):
        colour = task_colours["RT"]
    elif task_is_sink(taskname):
        colour = task_colours["sink"]

    return colour


def task_is_black_holes(name):
    """
    Does the task concern black holes?

    Parameters
    ----------

    name: str
        Task name
    """
    if "bh" in name or "bpart" in name or "swallow" in name:
        return True
    return False


def task_is_stars(name):
    """
    Does the task concern stars?

    Parameters
    ----------

    name: str
        Task name
    """
    if "stars" in name or "spart" in name:
        return True
    if "sf_count" in name:
        return True

    return False


def task_is_hydro(name):
    """
    Does the task concern the hydro?

    Parameters
    ----------

    name: str
        Task name
    """
    if "hydro" in name:
        return True
    if "_part" in name:
        return True
    if "density" in name and "stars" not in name and "bh" not in name:
        return True
    if "rho" in name and "bpart" not in name:
        return True
    if "gradient" in name and "rt_gradient" not in name:
        return True
    if "force" in name and "grav" not in name:
        return True
    if "xv" in name and "bpart" not in name:
        return True

    task_name = [
        "sort",
        "ghost_in",
        "ghost",
        "ghost_out",
        "extra_ghost",
        "cooling",
        "cooling_in",
        "cooling_out",
        "star_formation",
    ]
    if name in task_name:
        return True
    return False


def task_is_gravity(name):
    """
    Does the task concern the gravity?

    Parameters
    ----------

    name: str
        Task name
    """
    if "gpart" in name:
        return True
    if "grav" in name:
        return True
    return False


def task_is_RT(name):
    """
    Does the task concern Radiative Transfer?

    Parameters
    ----------

    name: str
        Task name
    """
    if "_rt" in name:
        return True
    elif name.startswith("rt_"):
        return True
    return False


def task_is_sink(name):
    """
    Does the task concern the sink particles?

    Parameters
    ----------

    name: str
        Task name
    """
    if "sink" in name:
        return True
    return False


def get_function_calls(name):
    txt = None
    if name == "ghost":
        txt = """hydro_end_density, chemistry_end_density,<br/>
        hydro_prepare_gradient, hydro_reset_gradient,<br/>
        hydro_prepare_force, hydro_reset_acceleration,<br/>
        hydro_init_part, chemistry_init_part,<br/>
        hydro_has_no_neighbours, chemistry_part_has_no_neighbours
        """

    elif name == "cooling":
        txt = "cooling_cool_part"

    elif name == "timestep":
        txt = "tracers_after_timestep"

    elif name == "drift_part":
        txt = """drift_part, tracers_after_drift,<br/>
        hydro_init_part, chemistry_init_part,<br/>
        tracers_after_init
        """

    elif name == "kick1":
        txt = "kick_part, kick_gpart, kick_spart"

    elif name == "kick2":
        txt = """kick_part, kick_gpart, kick_spart,<br/>
        hydro_reset_predicted_values,
        gravity_reset_predicted_Values,<br/>
        stars_reset_predicted_values,
        """

    elif name == "end_force":
        txt = """hydro_end_force, gravity_end_force,<br/>
        stars_end_force"""

    elif name == "drift_gpart":
        txt = """drift_gpart, gravity_init_gpart,<br/>
        drift_spart
        """

    if "density" in name and "stars" not in name:
        txt = """runner_iact_nonsym_chemistry, runner_iact_chemistry,<br/>
        runner_iact_nonsym_density, runner_iact_density"""

    if "force" in name and "end" not in name:
        txt = "runner_iact_nonsym_density, runner_iact_density"

    if txt is None:
        return None
    else:
        pre = "<" + name + "<BR/> <Font POINT-SIZE='10'>Calls: "
        app = "</Font>>"
        return pre + txt + app


def write_task(
    f,
    name,
    implicit,
    mpi,
    task_is_in_top,
    task_is_in_hydro_super,
    task_is_in_grav_super,
    cell_has_active_task,
    with_calls,
    with_levels,
):
    """
    Write the special task (e.g. implicit and mpi)

    Parameters
    ----------

    f: File
        File where to write the data

    name: str
        Task name

    implicit: int
        Is the task implicit

    mpi: int
        Is the task MPI related

    task_is_in_hydro_super: bool
        whether task is in top level cell

    task_is_in_hydro_super: bool
        whether task is in hydro super cell
    
    task_is_in_grav_super: bool
        whether task is in grav super cell

    cell_has_active_task: bool
        if True, the specific cell you are trying to plot
        the graph for has an active task of this type. 
        Otherwise it only unlocks a dependency of some 
        other cell

    with_calls: bool
        if True, write down the function calls

    with_levels: bool
        if True, write down level at which tasks are called
    """
    # generate text
    txt = "\t " + name + "["

    if not cell_has_active_task:
        # give this precedence over implicit tasks.
        # If you're this deep in debugging trouble,
        # you will most likely know which tasks are
        # implicit.
        txt += "style=filled,fillcolor=lightpink2,"
    else:
        if implicit:
            txt += "style=filled,fillcolor=grey90,"
    if mpi:
        txt += "shape=diamond,style=filled,fillcolor=azure,"
    if with_levels:
        levelstr = ""
        if task_is_in_top:
            levelstr = "top"
        if task_is_in_hydro_super and not task_is_in_grav_super:
            if len(levelstr) > 0:
                levelstr += " / "
            levelstr += "hydro super"
        if task_is_in_grav_super and not task_is_in_hydro_super:
            if len(levelstr) > 0:
                levelstr += " / "
            levelstr += "grav super"
        if task_is_in_grav_super and task_is_in_hydro_super:
            if len(levelstr) > 0:
                levelstr += " / "
            levelstr += "super"

        if (
            (not task_is_in_top)
            and (not task_is_in_grav_super)
            and (not task_is_in_hydro_super)
        ):
            levelstr = "below super"

        txt += "\n\t\tlabel=<\n"
        txt += '\t\t\t<TABLE BORDER="0" CELLBORDER="0" CELLSPACING="0">\n'
        txt += (
            '\t\t\t\t<TR> <TD> <B> <FONT POINT-SIZE="18">'
            + name
            + " </FONT> </B> </TD> </TR> <!-- task name -->\n"
        )
        txt += (
            '\t\t\t\t<TR> <TD> <FONT POINT-SIZE="18">'
            + levelstr
            + "</FONT> </TD> </TR> <!-- task level -->\n"
        )
        txt += "\t\t\t</TABLE>\n"
        txt += "\t\t\t>,\n\t\t"

    col = get_task_colour(name)

    txt += "color=%s," % col

    if with_calls:
        func = get_function_calls(name)
        if func is not None:
            txt += "label=" + func + ","

    # remove extra ','
    if txt[-1] == ",":
        txt = txt[:-1]
    txt += "];\n"

    # write it
    f.write(txt)


def write_header(f, data, git, opt):
    """
    Write the header and the special tasks

    Parameters
    ----------

    f: File
        File where to write the data

    data: DataFrame
        The dataframe to write

    git: str
        The git version

    opt: object
        The options provided to this script
    """
    # write header
    f.write("digraph task_dep {\n")
    f.write("\t # Header\n")
    f.write('\t label="Task dependencies for SWIFT %s";\n' % git)
    f.write("\t compound=true;\n")
    f.write("\t ratio=0.66;\n")
    f.write("\t node[nodesep=0.15, fontsize=18, penwidth=3.];\n")
    f.write("\t edge[fontsize=12, penwidth=0.5];\n")
    f.write("\t ranksep=0.8;\n")
    f.write("\n")

    # write the special task
    f.write("\t # Special tasks\n")
    N = len(data)
    written = []
    # do task in
    for i in range(N):
        ta = data["task_in"][i]
        if ta in written:
            continue

        written.append(ta)
        write_task(
            f,
            ta,
            data["implicit_in"][i],
            data["mpi_in"][i],
            data["task_in_is_top"][i] == 1,
            data["task_in_is_hydro_super"][i] == 1,
            data["task_in_is_grav_super"][i] == 1,
            True,
            opt.with_calls,
            opt.with_levels
        )
        # Note: In the case where you are plotting a single cell,
        # any tasks only gets an entry in tasks_in if the specific 
        # cell has an active task of that type.

    # do task out
    for i in range(N):
        tb = data["task_out"][i]
        if tb in written:
            continue

        written.append(tb)
        write_task(
            f,
            tb,
            data["implicit_out"][i],
            data["mpi_out"][i],
            data["task_out_is_top"][i] == 1,
            data["task_out_is_hydro_super"][i] == 1,
            data["task_out_is_grav_super"][i] == 1,
            data["cell_has_active_task"][i] == 1, 
            opt.with_calls,
            opt.with_levels
        )

    f.write("\n")


def write_cluster(f, tasks, cluster):
    """
    Write a single cluster

    Parameters
    ----------

    f: File
        File where to write the data

    tasks: list
        List of all tasks in the cluster

    cluster: str
        Cluster name

    """

    f.write("\t subgraph cluster%s {\n" % cluster)
    f.write('\t\t label="";\n')
    f.write('\t\t bgcolor="grey99";\n')
    for t in tasks:
        f.write("\t\t %s;\n" % t)
    f.write("\t };\n\n")


def write_clusters(f, data):
    """
    Write all the clusters

    Parameters
    ----------

    f: File
        File where to write the data

    data: DataFrame
        The dataframe to write
    """
    f.write("\t # Clusters\n")
    # get list of all the clusters
    clusters = data[["cluster_in", "cluster_out"]]
    clusters = np.unique(clusters)

    cluster_in = data["cluster_in"]
    cluster_out = data["cluster_out"]
    # loop over all clusters
    for cluster in clusters:
        # is it a cluster?
        if cluster == "None":
            continue

        # get all the task in current cluster
        ta = data["task_in"][cluster_in == cluster]
        tb = data["task_out"][cluster_out == cluster]

        # make them unique
        tasks = np.append(ta, tb)
        tasks = np.unique(tasks)

        # write current cluster
        write_cluster(f, tasks, cluster)

    f.write("\n")


def write_dependencies(f, data):
    """
    Write all the dependencies between tasks

    Parameters
    ----------

    f: File
        File where to write the data

    data: DataFrame
        The dataframe to write

    """
    f.write("\t # Dependencies\n")
    N = len(data)
    written = []
    max_rank = data["number_rank"].max()
    #  for i in range(N):
    for i, l in data.iterrows():
        # get data
        ta = l["task_in"]
        tb = l["task_out"]
        number_link = l["number_link"]

        # check if already done
        name = "%s_%s" % (ta, tb)
        if name in written:
            raise Exception("Found two same task dependencies")

        written.append(name)

        # write relation
        arrow = ",color=%s" % l["task_colour"]
        if l["number_rank"] != max_rank:
            arrow += ",style=dashed"
        f.write(
            "\t %s->%s[label=%i%s,fontcolor=%s]\n"
            % (ta, tb, number_link, arrow, l["task_colour"])
        )


def write_footer(f):
    """
    Write the footer

    Parameters
    ----------

    f: File
        File where to write the data
    """
    f.write("}")


def set_task_colours(data):
    """
    Set the value of the task colour for the plot if you
    want it non-black. the `task_colours` dict is defined
    at the top of this script.

    Additionally, in the case this script is used to plot
    the task dependencies of a specific cell only, mark
    whether the cell has an active task of any type found,
    or whether the task in the dependency list is only being
    unlocked by it.

    Parameters
    ----------

    data: DataFrame
        DataFrame of all the tasks


    Returns
    -------

    data: DataFrame
        modified DataFrame of all the tasks, now with more
        colour
    """

    N = len(data)

    for i in range(N):
        taskname = data["task_in"][i]
        col = get_task_colour(taskname)
        # set the colour
        data.loc[i, "task_colour"] = col

    return data


if __name__ == "__main__":

    args, files = parse_args()

    for f in files:
        # output
        basename = path.splitext(f)[0]
        dot_output = basename + ".dot"
        png_output = basename + ".png"

        # read file
        data = []
        git = None
        data = read_csv(f, delimiter=",", comment="#")
        git = get_git_version(f, git)

        data = set_task_colours(data)

        # write output
        with open(dot_output, "w") as f:
            write_header(f, data, git, args)

            write_clusters(f, data)

            write_dependencies(f, data)

            write_footer(f)

        call(["dot", "-Tpng", dot_output, "-o", png_output])

        print("You will find the graph in %s" % png_output)

        if args.with_calls:
            print("We recommand to use the python package xdot available on pypi:")
            print("  python3 -m xdot %s" % dot_output)
