import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell
import Quickshell.Io

ShellRoot {
    Window {
        id: window
        visible: true
        width: 1060
        height: 750
        minimumWidth: 800
        minimumHeight: 600
        title: "Hyprmist — cloud study"
        color: tone("background", systemPalette.window)
        onClosing: Qt.quit()
        property var frameTimes: []
        property bool measureFrames: false
        onFrameSwapped: { if (measureFrames && reveal.running) frameTimes.push(Date.now()); }

        property var themeColors: ({})
        property string themeName: "Loading theme"
        property string palettePath: Quickshell.env("MIST_PREVIEW_PALETTE") || (Quickshell.env("XDG_STATE_HOME") || Quickshell.env("HOME") + "/.local/state") + "/burl/scheme.json"
        property var overrides: ({})
        SystemPalette { id: systemPalette }
        function tone(name: string, fallback: color): color {
            return themeColors[name] ? "#" + themeColors[name] : fallback;
        }
        function mistColor(name: string, role: string): color {
            const value = overrides[name] || role;
            return value.startsWith("#") ? value : tone(value, systemPalette.highlight);
        }
        FileView {
            path: Qt.resolvedUrl("options.json")
            watchChanges: true
            onFileChanged: reload()
            onLoaded: {
                try {
                    const colors = JSON.parse(text()).colors || {};
                    for (const value of Object.values(colors)) {
                        if (typeof value !== "string" || !/^(#[0-9a-fA-F]{6}|[a-zA-Z][a-zA-Z0-9_]*)$/.test(value))
                            throw new Error("Use a theme role or #RRGGBB color");
                    }
                    window.overrides = colors;
                } catch (error) { console.warn("Mist options:", String(error)); }
            }
        }
        FileView {
            path: window.palettePath
            watchChanges: true
            onFileChanged: reload()
            onLoaded: {
                try {
                    const data = JSON.parse(text());
                    if (data.colours?.primary && data.colours?.secondary && data.colours?.tertiary) {
                        window.themeColors = data.colours;
                        window.themeName = data.name;
                    }
                } catch (error) { console.warn("Theme file is incomplete; retaining the current palette"); }
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 30
            spacing: 14
            Text {
                text: "Cloud study"
                color: window.tone("onSurface", systemPalette.text)
                font.pixelSize: 24
                font.weight: Font.Medium
            }
            Text {
                text: "Theme: " + window.themeName + " · volumetric mist · arrangement " + stage.seed
                color: window.tone("onSurfaceVariant", systemPalette.text)
                font.pixelSize: 14
            }
            Item {
                id: stage
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                property real progress: 0.48
                property bool incoming: true
                property int seed: 1
                readonly property real windowWidth: shape.currentIndex === 1 ? Math.min(width * 0.35, height * 0.42) : width * (shape.currentIndex === 0 ? 0.58 : 0.36)
                readonly property real windowHeight: height * (shape.currentIndex === 1 ? 0.58 : shape.currentIndex === 2 ? 0.40 : 0.54)
                readonly property real windowX: (width - windowWidth) / 2
                readonly property real windowY: (height - windowHeight) / 2
                function arrange(nextSeed: int): void { seed = nextSeed; }
                function randomize(): void { arrange(1 + Math.floor(Math.random() * 1000000)); }

                Rectangle {
                    anchors.fill: parent
                    radius: 16
                    gradient: Gradient {
                        GradientStop { position: 0; color: window.tone("surfaceContainerHigh", systemPalette.base) }
                        GradientStop { position: 1; color: window.tone("surfaceContainerLowest", systemPalette.window) }
                    }
                }
                Item {
                    id: content
                    width: stage.width
                    height: stage.height
                    visible: false
                    layer.enabled: true
                    Rectangle {
                        width: stage.windowWidth
                        height: stage.windowHeight
                        anchors.centerIn: parent
                        radius: 12
                        color: window.tone("surfaceContainer", systemPalette.base)
                        border.color: window.tone("outline", systemPalette.mid)
                        Column {
                            anchors.fill: parent
                            anchors.margins: 24
                            spacing: 16
                            Row {
                                spacing: 8
                                Repeater {
                                    model: [window.tone("primary", systemPalette.highlight), window.tone("secondary", systemPalette.highlight), window.tone("tertiary", systemPalette.highlight)]
                                    Rectangle { required property color modelData; width: 8; height: 8; radius: 4; color: modelData }
                                }
                            }
                            Text { text: "A window in the mist"; color: window.tone("onSurface", systemPalette.text); font.pixelSize: stage.windowWidth < 450 ? 18 : 24 }
                            Text { text: "The mist passes. The window stays still."; color: window.tone("onSurfaceVariant", systemPalette.text); font.pixelSize: stage.windowWidth < 450 ? 11 : 15 }
                            Repeater {
                                model: [0.9, 0.75, 0.83, 0.55]
                                Rectangle { required property real modelData; width: stage.windowWidth * 0.72 * modelData; height: 7; radius: 3; color: window.tone("surfaceContainerHighest", systemPalette.mid) }
                            }
                        }
                    }
                }
                ShaderEffect {
                    id: fogVolume
                    anchors.fill: parent
                    visible: false
                    layer.enabled: true
                    layer.textureSize: Qt.size(Math.ceil(width * 0.85), Math.ceil(height * 0.85))
                    property real progress: stage.progress
                    property real seed: stage.seed
                    property real incoming: stage.incoming ? 1 : 0
                    property vector2d targetSize: Qt.vector2d(stage.width, stage.height)
                    property vector4d windowRect: Qt.vector4d(stage.windowX, stage.windowY, stage.windowWidth, stage.windowHeight)
                    property color shadowColor: window.mistColor("shadow", "surfaceContainerHighest")
                    property color accentA: window.mistColor("primary", "primary")
                    property color accentB: window.mistColor("secondary", "secondary")
                    property color accentC: window.mistColor("tertiary", "tertiary")
                    property color lightColor: window.mistColor("light", "onSurface")
                    fragmentShader: "volume.frag.qsb"
                }
                ShaderEffect {
                    anchors.fill: parent
                    property var source: content
                    property var smokeVolume: fogVolume
                    property real progress: stage.progress
                    property real seed: stage.seed
                    property real incoming: stage.incoming ? 1 : 0
                    property vector2d targetSize: Qt.vector2d(stage.width, stage.height)
                    property vector4d windowRect: Qt.vector4d(stage.windowX, stage.windowY, stage.windowWidth, stage.windowHeight)
                    property color shadowColor: fogVolume.shadowColor
                    property color accentA: fogVolume.accentA
                    property color accentB: fogVolume.accentB
                    property color accentC: fogVolume.accentC
                    property color lightColor: fogVolume.lightColor
                    fragmentShader: "composite.frag.qsb"
                }
                NumberAnimation {
                    id: reveal
                    target: stage
                    property: "progress"
                    from: 0
                    to: 1
                    duration: slow.checked ? 2700 : 900
                    onStopped: window.measureFrames = false
                }
                Component.onCompleted: { randomize(); console.log("MIST-PREVIEW-READY"); }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 16
                Button {
                    text: "Reveal"
                    onClicked: { reveal.stop(); stage.incoming = true; stage.randomize(); reveal.from = 0; reveal.to = 1; reveal.start(); }
                }
                Button {
                    text: "Conceal"
                    onClicked: { reveal.stop(); stage.incoming = false; reveal.from = 0; reveal.to = 1; reveal.start(); }
                }
                Button { text: "New arrangement"; onClicked: { reveal.stop(); stage.randomize(); } }
                ComboBox { id: shape; model: ["Wide", "Tall", "Compact"] }
                CheckBox { id: slow; text: "Slow motion"; checked: true }
                Slider {
                    Layout.fillWidth: true
                    from: 0
                    to: 1
                    value: stage.progress
                    onMoved: { reveal.stop(); stage.progress = value; }
                }
            }
            Text {
                text: "Preview only. Your desktop smoke effect remains off."
                color: window.tone("onSurfaceVariant", systemPalette.text)
                font.pixelSize: 12
            }
        }
        IpcHandler {
            target: "mistPreview"
            function state(): string {
                return JSON.stringify({seed: stage.seed, renderer: "volume",
                    width: stage.windowWidth, height: stage.windowHeight, progress: stage.progress, incoming: stage.incoming,
                    theme: window.themeName, primary: window.mistColor("primary", "primary"),
                    overrides: window.overrides});
            }
            function paletteFile(path: string): void { window.palettePath = path; }
            function benchmark(): void { reveal.stop(); window.frameTimes = []; window.measureFrames = true; stage.progress = 0; reveal.start(); }
            function timings(): string { return JSON.stringify(window.frameTimes); }
            function mode(revealing: bool): void { reveal.stop(); stage.incoming = revealing; }
            function arrange(value: int): void { reveal.stop(); stage.arrange(value); }
            function windowShape(value: int): void { shape.currentIndex = value; }
            function progress(value: real): void { reveal.stop(); stage.progress = value; }
            function capture(path: string): void {
                stage.grabToImage(result => { result.saveToFile(path); console.log("MIST-PREVIEW-CAPTURED", path); });
            }
        }
    }
}
