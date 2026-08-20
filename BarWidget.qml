import QtQuick
import Quickshell
import Quickshell.Io
import qs.Commons
import qs.Ui

BarWidget {
  id: root
  moduleName: "io.github.rawritude.floating-mode"

  property bool floatingMode: false
  property bool busy: false
  property string lastError: ""
  readonly property string windowGlyph: String.fromCodePoint(0xF05B2)
  readonly property string configHome: Quickshell.env("XDG_CONFIG_HOME") !== ""
    ? Quickshell.env("XDG_CONFIG_HOME") : Quickshell.env("HOME") + "/.config"
  readonly property string helper: configHome
    + "/omarchy/plugins/io.github.rawritude.floating-mode/bin/floating-mode"

  implicitWidth: button.implicitWidth
  implicitHeight: button.implicitHeight

  function refresh() {
    if (!statusProc.running) statusProc.running = true
  }

  function setMode(want) {
    if (actionProc.running) return
    busy = true
    lastError = ""
    actionProc.command = [root.helper, want ? "on" : "off"]
    actionProc.running = true
  }

  function toggleMode() {
    setMode(!floatingMode)
  }

  Process {
    id: statusProc
    command: [root.helper, "status"]
    stdout: StdioCollector {
      waitForEnd: true
      onStreamFinished: root.floatingMode = String(text || "").trim() === "on"
    }
  }

  Process {
    id: actionProc
    stderr: StdioCollector {
      waitForEnd: true
      onStreamFinished: root.lastError = String(text || "").trim()
    }
    onExited: function(code) {
      root.busy = false
      if (code !== 0 && root.lastError === "") root.lastError = "Could not change window mode"
      root.refresh()
    }
  }

  Timer {
    interval: 1200
    running: true
    repeat: true
    triggeredOnStart: true
    onTriggered: root.refresh()
  }

  BarIconButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    text: root.windowGlyph
    active: root.floatingMode
    activeColor: Color.accent
    dimmed: root.busy
    tooltipText: root.lastError !== ""
      ? root.lastError
      : (root.floatingMode ? "Floating mode ON — click to tile" : "Floating mode OFF — click to float")
    onPressed: function(buttonCode) { root.toggleMode() }
  }
}
