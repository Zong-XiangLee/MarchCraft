import QtQuick
import QtQuick.Controls
import QtTest
import "../qml"

TestCase {
    id: test
    name: "MovementTabs"
    width: 800; height: 300; visible: true
    when: windowShown
    QtObject {
        id: mockProject
        property var movements: [{id: "a", name: "Opener"}, {id: "b", name: "Ballad"}]
        property int currentMovementIndex: 0
        readonly property int movementCount: movements.length
        readonly property string currentMovementName: movements[currentMovementIndex].name
        function activateMovement(index) { currentMovementIndex = index }
        function createMovement(name, duplicate) {
            movements = movements.concat([{id: "new", name: name}]); currentMovementIndex = movements.length - 1
            return true
        }
        function renameMovement(index, name) {
            let copy = movements.slice(); copy[index] = {id: copy[index].id, name: name}; movements = copy
            return true
        }
    }
    MovementTabs { id: tabs; project: mockProject; width: parent.width; height: 32 }
    function init() {
        mockProject.movements = [{id: "a", name: "Opener"}, {id: "b", name: "Ballad"}]
        mockProject.currentMovementIndex = 0
        wait(20)
    }
    function test_clickSwitchesMovement() {
        const ballad = findChild(tabs, "movementTab1")
        mouseClick(ballad, ballad.width / 2, 16)
        compare(mockProject.currentMovementIndex, 1)
    }
    function test_createAndRenameThroughDialog() {
        mouseClick(findChild(tabs, "addMovementButton"))
        const dialog = findChild(test, "movementNameDialog")
        const field = findChild(test, "movementNameField")
        verify(dialog.visible)
        field.text = "Finale"
        mouseClick(dialog.standardButton(Dialog.Ok))
        compare(mockProject.movementCount, 3); compare(mockProject.currentMovementName, "Finale")
        tabs.openName("rename"); field.text = "Closing movement"
        mouseClick(dialog.standardButton(Dialog.Ok))
        compare(mockProject.currentMovementName, "Closing movement")
    }
}
