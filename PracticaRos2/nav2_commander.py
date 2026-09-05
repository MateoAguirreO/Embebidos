import rclpy
from rclpy.node import Node
from rclpy.action import ActionClient
from nav2_msgs.action import NavigateToPose
from geometry_msgs.msg import PoseStamped


class Nav2Commander(Node):
    def __init__(self):
        super().__init__('nav2_commander_node')
        # Inicializar el Action Client conectado al servidor 'navigate_to_pose'
        self._action_client = ActionClient(self, NavigateToPose, 'navigate_to_pose')

    def send_goal(self, x, y, theta_z, theta_w):
        self.get_logger().info('Esperando al servidor de navegación Nav2...')
        self._action_client.wait_for_server()

        # Construir el mensaje de destino (Goal)
        goal_msg = NavigateToPose.Goal()
        goal_msg.pose.header.frame_id = 'map'
        goal_msg.pose.header.stamp = self.get_clock().now().to_msg()

        # Posición espacial (Metros)
        goal_msg.pose.pose.position.x = float(x)
        goal_msg.pose.pose.position.y = float(y)

        # Orientación usando Cuaterniones
        goal_msg.pose.pose.orientation.z = float(theta_z)
        goal_msg.pose.pose.orientation.w = float(theta_w)

        self.get_logger().info(f'Enviando objetivo: X={x}, Y={y}')

        # Enviar el objetivo de forma asíncrona
        self._send_goal_future = self._action_client.send_goal_async(goal_msg)
        self._send_goal_future.add_done_callback(self.goal_response_callback)

    def goal_response_callback(self, future):
        goal_handle = future.result()
        if not goal_handle.accepted:
            self.get_logger().info('El objetivo fue rechazado por Nav2.')
            return

        self.get_logger().info('Objetivo aceptado, el robot está en movimiento...')
        self._get_result_future = goal_handle.get_result_async()
        self._get_result_future.add_done_callback(self.get_result_callback)

    def get_result_callback(self, future):
        status = future.result().status
        self.get_logger().info(f'Navegación finalizada con código de estado: {status}')
        # Finalizar el nodo tras llegar al destino
        rclpy.shutdown()


def main(args=None):
    rclpy.init(args=args)
    nav_commander = Nav2Commander()

    # Coordenadas de prueba (Configurar según tu mapa)
    # X=1.5, Y=0.5, y orientación Z=0.0, W=1.0 (apuntando hacia adelante)
    nav_commander.send_goal(0.3, 0.5, 0.2, 0.5)

    # Mantener el nodo vivo procesando callbacks
    rclpy.spin(nav_commander)


if __name__ == '__main__':
    main()
