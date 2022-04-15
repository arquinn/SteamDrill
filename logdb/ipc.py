import pydot

# our modules
import opinfo

class IPCGraph(object):
    def __init__(self):
        self.nodes = []

    def add_node(self, group_id, pid, cmd=""):
        node = IPCNode(group_id, pid, cmd=cmd)
	assert(node)
        self.nodes.append(node)

    def has_node(self, group_id, pid):
        for node in self.nodes:
            if node.group_id == group_id and node.pid == pid:
                return True
        return False

    def get_node(self, group_id, pid):
        for node in self.nodes:
            if node.group_id == group_id and node.pid == pid:
                return node
        return None

    def visualize_graph(self, output_file="/tmp/output.dot"):
        graph = pydot.Dot(graph_type='digraph')

        # go through each node and create a pydot Node
        pynodes = {}
        for node in self.nodes:
            name = "Group " + str(node.group_id) + " Pid " + str(node.pid)
            name += "\n" + node.cmd
            pynode = pydot.Node(name, shape="box")
            pynodes[node] = pynode
            graph.add_node(pynode)

        for node in self.nodes:
            for edge in node.edges:
                pydot_edge = pydot.Edge(pynodes[edge.from_node], pynodes[edge.to_node])
                # pydot_edge.set_label("%s , %s" % (str(edge.write_info), str(edge.read_info)))
                if edge.edge_type == "EXEC":
                    pydot_edge.set_color("black")
                elif edge.edge_type == "RW":
                    pydot_edge.set_color("red")
                else:
                    pydot_edge.set_color("green")
                graph.add_edge(pydot_edge)

        graph.write(output_file)

    def output_graph(self):
        root_nodes = []
        printed_nodes = {}

        # find root node(s) aka nodes with in-degree 0
        for node in self.nodes:
            if len(node.edges_to) == 0:
                root_nodes.append(node)

        queue = []
        for node in root_nodes:
            queue.append(node)
        while queue:
            node = queue.pop()
            if node in printed_nodes:
                continue
            print(str(node))
            for edge in node.edges:
                queue.append(edge.from_node)

class IPCNode(object):
    '''
    Each node corresponds to a process
    '''
    def __init__(self, group_id, pid, cmd=""):
        self.group_id = group_id
        self.pid = pid
        self.cmd = cmd

        # Reads in this process
        self.read_infos = []

        # Writes in this process
        self.write_infos = []

        # execs in this process
        self.exec_infos = []

        # edges out of this node
        self.edges = []

        # edges pointing to this node
        self.edges_to = []

    def add_edge(self, to_node, write_info, read_info, edge_type=None):
        edge = IPCEdge(self, to_node, write_info, read_info, edge_type=edge_type)
        self.edges.append(edge)
        to_node.edges_to.append(edge)
    
    def has_edge(self, to_node):
        for edge in self.edges:
            if edge.to_node == to_node:
                return True
        return False

    def add_read(self, read_info):
        assert isinstance(read_info, opinfo.ReadInfo)
        assert read_info.group_id == self.group_id
        assert read_info.pid == self.pid
        self.read_infos.append(read_info)

    def add_write(self, write_info):
        assert isinstance(write_info, opinfo.WriteInfo)
        assert write_info.group_id == self.group_id
        assert write_info.pid == self.pid
        self.write_infos.append(write_info)

    def add_exec(self, exec_info):
        assert isinstance(exec_info, opinfo.ExecInfo)
        assert exec_info.group_id == self.group_id
        assert exec_info.pid == self.pid
        self.exec_infos.append(exec_info)

    def __hash__(self):
        # sure why not have the group id and pid uniquely identify a node
        return hash((self.group_id, self.pid))

    def __eq__(self, other):
        return (self.group_id, self.pid) == (other.group_id, other.pid)

    def __str__(self):
        return str(self.__dict__)

class IPCEdge(object):
    def __init__(self, from_node, to_node, write_info, read_info, edge_type=None):
        self.from_node = from_node
        self.to_node = to_node
        self.write_info = write_info
        self.read_info = read_info
        self.edge_type = edge_type
