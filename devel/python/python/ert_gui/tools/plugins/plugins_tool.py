from PyQt4.QtGui import QMenu
from ert_gui.models.connectors.init import CaseList
from ert_gui.tools import Tool
from ert_gui.widgets import util
from ert_gui.tools.plugins import PluginHandler, PluginRunner


class PluginsTool(Tool):
    def __init__(self, plugin_handler):
        """
        @type plugin_handler: PluginHandler
        """
        enabled = len(plugin_handler) > 0
        super(PluginsTool, self).__init__("Plugins", "tools/plugins", util.resourceIcon("ide/plugin"), enabled, popup_menu=True)

        self.__plugins = {}

        menu = QMenu()
        for plugin in plugin_handler:
            plugin_runner = PluginRunner(plugin)
            plugin_runner.setPluginFinishedCallback(self.trigger)

            self.__plugins[plugin] = plugin_runner
            plugin_action = menu.addAction(plugin.getName())
            plugin_action.setToolTip(plugin.getDescription())
            plugin_action.triggered.connect(plugin_runner.run)

        self.getAction().setMenu(menu)


    def trigger(self):
        CaseList().externalModificationNotification() # plugin may have added new cases.

