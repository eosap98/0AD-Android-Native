// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise the actual bundled session script with the mouse protocol used by
// the Android adapter. Engine picking/commands are fixtures, not a running game.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const inputSource = fs.readFileSync(process.argv[2], 'utf8');
const adapter = fs.readFileSync(process.argv[3], 'utf8');
const selectionSource = fs.readFileSync(process.argv[4], 'utf8');
const cancelSource = adapter.match(/CancelSelectionScript = R"JS\(([\s\S]*?)\)JS"/)[1];
const deselectSource = adapter.match(/DeselectScript = R"JS\(([\s\S]*?)\)JS"/)[1];
const selectTapSource = adapter.match(/SelectTapScript = R"JS\(([\s\S]*?)\)JS"/)[1];
let passed = 0;

function fixture() {
    const commands = [];
    const builds = [];
    const similar = [];
    const rects = [];
    const bandbox = {hidden: true};
    let picked = 17, validPlacement = true, enoughResources = true, actionType = 'move';
    const states = new Map([[17, {player: 1, template: 'units/spearman_b', identity: {
        classes: ['Unit'], selectionGroupName: 'units/spearman'
    }}]]);
    for (const id of [23, 41, 99])
        states.set(id, {player: 1, template: 'units/spearman_a', identity: {classes: ['Unit'], selectionGroupName: 'units/spearman'}});
    const placement = {
        mode: undefined,
        Reset() { this.mode = undefined; this.template = undefined; this.position = undefined; },
        SetDefaultAngle() { this.angle = 0; },
    };
    let menuCloses = 0;
    class Vector2D {
        constructor(x, y) { this.x = x; this.y = y; }
        distanceTo(p) { return Math.hypot(p.x - this.x, p.y - this.y); }
        distanceToSquared(p) { return (p.x - this.x) ** 2 + (p.y - this.y) ** 2; }
        static min(a, b) { return new Vector2D(Math.min(a.x, b.x), Math.min(a.y, b.y)); }
        static max(a, b) { return new Vector2D(Math.max(a.x, b.x), Math.max(a.y, b.y)); }
    }
    const context = vm.createContext({
        Vector2D, console,
        Engine: {
            HotkeyIsPressed: () => false,
            ConfigDB_GetValue: (_, key) => key === 'gui.scale' ? '1' : key === 'gui.session.snaptoedges' ? 'disabled' : '4',
            GetGUIObjectByName: name => { assert.equal(name, 'bandbox'); return bandbox; },
            PickEntityAtPoint: () => picked,
            PickSimilarPlayerEntities: (...args) => { similar.push(args); return [17, 23, 41]; },
            PickPlayerEntitiesInRect: (...args) => { rects.push(args); return [17, 23]; },
            GetFollowedEntity: () => 0,
            CameraFollow: () => {},
            GetTerrainAtScreenPoint: (x, y) => ({x, z: y}),
            PostNetworkCommand: command => builds.push(JSON.parse(JSON.stringify(command))),
            GuiInterfaceCall: name => {
                if (name === 'GetNeededResources') return enoughResources ? undefined : {wood: 100};
                if (name === 'GetFoundationSnapData' || name === 'PlaySound') return undefined;
                if (['SetSelectionHighlight', 'SetStatusBars', 'SetMotionDebugOverlay'].includes(name)) return undefined;
                if (name === 'SetBuildingPlacementPreview') return {success: validPlacement};
                if (name === 'SetWallPlacementPreview') return validPlacement ? {
                    pieces: [{template: 'wall/long', x: 100, z: 100, angle: 0}],
                    startSnappedEnt: 0, endSnappedEnt: 0,
                } : false;
                throw new Error('Unexpected GUI service: ' + name);
            },
        },
        GetSimState: () => ({cinemaPlaying: false}),
        GetEntityState: id => states.get(id),
        GetTemplateData: () => ({cost: {wood: 100}}),
        hasClass: (entity, name) => entity.identity.classes.includes(name),
        controlsPlayer: () => true,
        g_ViewedPlayer: 1,
        g_IsReplay: false,
        g_IsObserver: false,
        g_SimState: {players: [{}, {}]},
        registerConfigChangeHandler: () => {},
        resetIdleUnit: () => {},
        g_AutoFormation: {getNull: () => 'special/formations/null'},
        g_Menu: {close: () => ++menuCloses},
        placementSupport: placement,
    });
    vm.runInContext(selectionSource, context, {filename: 'packaged/gui/session/selection.js'});
    const selection = context.g_Selection;
    selection.addList([99]);
    selection.dirty = false;
    const selected = selection.selected;
    vm.runInContext(inputSource, context, {filename: 'packaged/gui/session/input.js'});
    context.determineAction = (x, y) => selection.size() && actionType ? {type: actionType, x, y} : undefined;
    context.doAction = action => { commands.push(action); return true; };
    const evaluate = code => vm.runInContext(code, context);
    const event = (type, x = 100, y = 100, button = 1, clicks = 1) => {
        const value = {type, x, y, button, clicks};
        if (!context.handleInputBeforeGui(value, null)) context.handleInputAfterGui(value);
    };
    const beginBuilding = (wall = false) => {
        Object.assign(placement, {mode: wall ? 'wall' : 'building', template: 'structures/house',
            angle: 0, actorSeed: 1, wallSet: {templates: {tower: 'wall/tower'}}});
        evaluate('inputState = INPUT_BUILDING_PLACEMENT;');
    };
    const trySelectTap = (x = 100, y = 100) => {
        event('mousemotion', x, y);
        if (!evaluate(selectTapSource)) return false;
        event('mousebuttondown', x, y);
        event('mousebuttonup', x, y);
        return true;
    };
    return {selected, selection, commands, builds, similar, rects, bandbox, evaluate, event, states,
        placement, beginBuilding, trySelectTap,
        setAction: type => { actionType = type; },
        setPicked: id => { picked = id; }, setValidPlacement: valid => { validPlacement = valid; },
        setResources: available => { enoughResources = available; }, menuCloses: () => menuCloses};
}

function test(name, callback) {
    callback(fixture());
    ++passed;
    console.log('PASS ' + name);
}

test('right-button command preserves selected units and returns to normal', f => {
    f.event('mousemotion', 150, 200);
    f.event('mousebuttondown', 150, 200, 3);
    assert.equal(f.commands.length, 0);
    f.event('mousebuttonup', 150, 200, 3);
    assert.deepEqual(f.commands, [{type: 'move', x: 150, y: 200}]);
    assert.deepEqual([...f.selected], [99]);
    assert.equal(f.evaluate('inputState'), 0);
});

test('one short tap selects a builder and signals the real selection HUD to refresh', f => {
    f.states.get(17).builder = {entities: ['structures/house']};
    f.evaluate('g_allBuildableEntities = ["stale"];');
    f.event('mousemotion');
    assert.equal(f.selection.dirty, false, 'Hover alone must not update the selection HUD');
    assert.equal(f.trySelectTap(), true);
    assert.deepEqual([...f.selected], [17]);
    assert.equal(f.selection.dirty, true);
    assert.equal(f.evaluate('g_allBuildableEntities'), undefined);
    assert.equal(f.commands.length, 0);
    assert.equal(f.evaluate('inputState'), 0);
});

test('a later short tap switches the selected unit and refreshes its action panel', f => {
    assert.equal(f.trySelectTap(), true);
    f.selection.dirty = false;
    f.setPicked(23);
    assert.equal(f.trySelectTap(210, 240), true);
    assert.deepEqual([...f.selected], [23]);
    assert.equal(f.selection.dirty, true);
    assert.equal(f.commands.length, 0);
});

test('one short tap selects an owned building for its training and build menus', f => {
    f.states.get(17).identity.classes = ['Structure'];
    f.states.get(17).trainer = {entities: ['units/spearman_b']};
    f.evaluate('g_allTrainableEntities = ["stale"];');
    assert.equal(f.trySelectTap(), true);
    assert.deepEqual([...f.selected], [17]);
    assert.equal(f.selection.dirty, true);
    assert.equal(f.evaluate('g_allTrainableEntities'), undefined);
    assert.equal(f.commands.length, 0);
});

test('a building rally-point action does not prevent tapping an owned unit to select it', f => {
    f.setAction('set-rallypoint');
    assert.equal(f.trySelectTap(), true);
    assert.deepEqual([...f.selected], [17]);
    assert.equal(f.commands.length, 0);
});

test('double tap still expands a short-tap selection using the real selection group', f => {
    assert.equal(f.trySelectTap(), true);
    f.event('mousebuttondown'); f.event('mousebuttonup', 100, 100, 1, 2);
    assert.deepEqual([...f.selected], [17, 23, 41]);
    assert.equal(f.similar.length, 1);
    assert.equal(f.commands.length, 0);
});

test('a tap with no selection can inspect an enemy without issuing a command', f => {
    f.selection.reset();
    f.states.get(17).player = 2;
    assert.equal(f.trySelectTap(), true);
    assert.deepEqual([...f.selected], [17]);
    assert.equal(f.commands.length, 0);
});

test('a tap on empty ground retains the selected units and the move command', f => {
    f.setPicked(0);
    assert.equal(f.trySelectTap(), false);
    assert.deepEqual([...f.selected], [99]);
    assert.equal(f.selection.dirty, false);
    f.event('mousebuttondown', 100, 100, 3); f.event('mousebuttonup', 100, 100, 3);
    assert.equal(f.commands.length, 1);
    assert.equal(f.commands[0].type, 'move');
});

test('enemy tap keeps the attack command and the selected army', f => {
    f.states.get(17).player = 2; f.setAction('attack');
    assert.equal(f.trySelectTap(), false);
    f.event('mousebuttondown', 100, 100, 3); f.event('mousebuttonup', 100, 100, 3);
    assert.deepEqual([...f.selected], [99]);
    assert.equal(f.commands.length, 1);
    assert.equal(f.commands[0].type, 'attack');
});

for (const action of ['gather', 'repair', 'heal', 'returnresource', 'setup-trade-route']) {
    test('specific ' + action + ' action on an owned target remains a command', f => {
        f.setAction(action);
        assert.equal(f.trySelectTap(), false);
        f.event('mousebuttondown', 100, 100, 3); f.event('mousebuttonup', 100, 100, 3);
        assert.deepEqual([...f.selected], [99]);
        assert.equal(f.selection.dirty, false);
        assert.equal(f.commands.length, 1);
        assert.equal(f.commands[0].type, action);
    });
}

test('a vanished entity cannot be selected by the short-tap path', f => {
    f.states.delete(17);
    assert.equal(f.trySelectTap(), false);
    assert.deepEqual([...f.selected], [99]);
    assert.equal(f.selection.dirty, false);
    assert.equal(f.commands.length, 0);
});

test('building placement cannot be turned into a short-tap unit selection', f => {
    f.beginBuilding();
    assert.equal(f.trySelectTap(), false);
    assert.deepEqual([...f.selected], [99]);
    assert.equal(f.evaluate('inputState === INPUT_BUILDING_PLACEMENT'), true);
    assert.equal(f.builds.length, 0);
});

test('two-finger deselect clears selection without a unit command or menu event', f => {
    f.evaluate(deselectSource);
    assert.equal(f.selected.size, 0);
    assert.equal(f.commands.length, 0);
    assert.equal(f.menuCloses(), 0);
});

test('double-tap release selects the on-screen selection group, including different ranks', f => {
    f.event('mousemotion');
    f.event('mousebuttondown');
    f.event('mousebuttonup', 100, 100, 1, 2);
    assert.deepEqual(f.similar, [['units/spearman', false, false, false]]);
    assert.deepEqual([...f.selected], [17, 23, 41]);
    assert.equal(f.commands.length, 0);
    assert.equal(f.evaluate('inputState'), 0);
});

test('double tap picks the current unit instead of a stale earlier selection', f => {
    f.evaluate('clickedEntity = 99;');
    f.event('mousebuttondown');
    f.event('mousebuttonup', 100, 100, 1, 2);
    assert.deepEqual(f.similar[0], ['units/spearman', false, false, false]);
    assert.equal(f.evaluate('clickedEntity'), 17);
});

test('double tap falls back to the exact template without a selection group', f => {
    delete f.states.get(17).identity.selectionGroupName;
    f.event('mousebuttondown');
    f.event('mousebuttonup', 100, 100, 1, 2);
    assert.deepEqual(f.similar[0], ['units/spearman_b', false, true, false]);
    assert.deepEqual([...f.selected], [17, 23, 41]);
});

test('single hold still selects just the tapped unit', f => {
    f.event('mousebuttondown');
    f.event('mousebuttonup');
    assert.deepEqual([...f.selected], [17]);
    assert.equal(f.similar.length, 0);
});

test('double tap on empty ground cannot send a unit command', f => {
    f.setPicked(0);
    f.event('mousebuttondown');
    f.event('mousebuttonup', 100, 100, 1, 2);
    assert.equal(f.commands.length, 0);
    assert.equal(f.similar.length, 0);
    assert.equal(f.evaluate('inputState'), 0);
});

test('a unit disappearing before double-tap release is handled safely', f => {
    f.event('mousebuttondown');
    f.states.delete(17);
    f.event('mousebuttonup', 100, 100, 1, 2);
    assert.equal(f.similar.length, 0);
    assert.equal(f.commands.length, 0);
    assert.equal(f.evaluate('inputState'), 0);
});

test('held primary click selects one unit', f => {
    f.event('mousemotion');
    f.event('mousebuttondown');
    f.event('mousebuttonup');
    assert.deepEqual([...f.selected], [17]);
    assert.equal(f.commands.length, 0);
});

test('held drag selects the final rectangle even with only a release displacement', f => {
    f.event('mousemotion', 100, 100);
    f.event('mousebuttondown', 100, 100);
    f.event('mousemotion', 220, 240);
    f.event('mousemotion', 220, 240);
    assert.equal(f.bandbox.hidden, false);
    f.event('mousebuttonup', 220, 240);
    assert.deepEqual([...f.selected], [17, 23]);
    assert.deepEqual(f.rects.at(-1).slice(0, 4), [100, 100, 220, 240]);
    assert.equal(f.bandbox.hidden, true);
    assert.equal(f.evaluate('inputState'), 0);
    assert.equal(f.commands.length, 0);
});

test('box dragged up and left uses the correct bounds', f => {
    f.event('mousemotion', 300, 250);
    f.event('mousebuttondown', 300, 250);
    f.event('mousemotion', 100, 90);
    f.event('mousemotion', 100, 90);
    f.event('mousebuttonup', 100, 90);
    assert.deepEqual(f.rects.at(-1).slice(0, 4), [100, 90, 300, 250]);
    assert.deepEqual([...f.selected], [17, 23]);
});

test('cancelling a held click preserves the earlier selection', f => {
    f.event('mousemotion');
    f.event('mousebuttondown');
    f.evaluate(cancelSource);
    assert.deepEqual([...f.selected], [99]);
    assert.equal(f.evaluate('inputState'), 0);
    assert.equal(f.evaluate('clickedEntity'), 0);
});

test('cancelling a visible box hides it without committing units', f => {
    f.event('mousemotion'); f.event('mousebuttondown');
    f.event('mousemotion', 220, 240); f.event('mousemotion', 220, 240);
    f.evaluate(cancelSource);
    assert.equal(f.bandbox.hidden, true);
    assert.deepEqual([...f.selected], [99]);
    assert.equal(f.evaluate('inputState'), 0);
    assert.equal(f.commands.length, 0);
});

for (const state of ['INPUT_BUILDING_CLICK', 'INPUT_BUILDING_DRAG', 'INPUT_BUILDING_WALL_CLICK']) {
    test('cancelling ' + state + ' restores placement without a stuck click', f => {
        f.evaluate('inputState = ' + state + ';');
        f.evaluate(cancelSource);
        assert.equal(f.evaluate('inputState === INPUT_BUILDING_PLACEMENT'), true);
        assert.deepEqual([...f.selected], [99]);
        assert.equal(f.commands.length, 0);
    });
}

test('ordinary GUI cancellation does not clear unit selection', f => {
    f.evaluate(cancelSource);
    assert.deepEqual([...f.selected], [99]);
    assert.equal(f.evaluate('inputState'), 0);
});

test('the first placement tap and drag only move the preview', f => {
    f.beginBuilding();
    f.event('mousemotion', 100, 120);
    f.event('mousemotion', 240, 280);
    assert.deepEqual(f.placement.position, {x: 240, z: 280});
    assert.equal(f.builds.length, 0);
    assert.equal(f.evaluate('inputState === INPUT_BUILDING_PLACEMENT'), true);
    assert.deepEqual([...f.selected], [99]);
});

test('confirmed placement sends exactly one construction order at the tapped location', f => {
    f.beginBuilding();
    f.event('mousemotion', 220, 230);
    f.event('mousebuttondown', 220, 230);
    assert.equal(f.builds.length, 0);
    f.event('mousebuttonup', 220, 230);
    assert.equal(f.builds.length, 1);
    assert.equal(f.builds[0].type, 'construct');
    assert.equal(f.builds[0].template, 'structures/house');
    assert.deepEqual(f.builds[0].entities, [99]);
    assert.equal(f.builds[0].x, 220);
    assert.equal(f.builds[0].z, 230);
    assert.equal(f.evaluate('inputState'), 0);
    assert.equal(f.commands.length, 0);
});

test('an invalid confirmed location keeps placement active without spending resources', f => {
    f.beginBuilding();
    f.setValidPlacement(false);
    f.event('mousemotion'); f.event('mousebuttondown'); f.event('mousebuttonup');
    assert.equal(f.builds.length, 0);
    assert.equal(f.evaluate('inputState === INPUT_BUILDING_PLACEMENT'), true);
});

test('running out of resources while previewing exits placement without a click', f => {
    f.beginBuilding(); f.setResources(false); f.event('mousemotion');
    assert.equal(f.evaluate('inputState'), 0);
    assert.equal(f.builds.length, 0);
    assert.deepEqual([...f.selected], [99]);
});

test('two-finger deselect cancels a building preview and clears the builders', f => {
    f.beginBuilding(); f.event('mousemotion'); f.evaluate(deselectSource);
    assert.equal(f.evaluate('inputState'), 0);
    assert.equal(f.placement.mode, undefined);
    assert.equal(f.selected.size, 0);
    assert.equal(f.builds.length, 0);
});

test('wall previews require confirmation for both the start and endpoint', f => {
    f.beginBuilding(true);
    f.event('mousemotion');
    assert.equal(f.builds.length, 0);
    f.event('mousebuttondown'); f.event('mousebuttonup');
    assert.equal(f.evaluate('inputState === INPUT_BUILDING_WALL_PATHING'), true);
    f.event('mousemotion', 300, 300);
    assert.equal(f.builds.length, 0);
    f.event('mousebuttondown', 300, 300); f.event('mousebuttonup', 300, 300);
    assert.equal(f.builds.length, 1);
    assert.equal(f.builds[0].type, 'construct-wall');
    assert.equal(f.evaluate('inputState'), 0);
});

test('two-finger deselect also cancels an unfinished wall', f => {
    f.beginBuilding(true);
    f.event('mousemotion'); f.event('mousebuttondown'); f.event('mousebuttonup');
    f.evaluate(deselectSource);
    assert.equal(f.evaluate('inputState'), 0);
    assert.equal(f.selected.size, 0);
    assert.equal(f.builds.length, 0);
});

console.log(passed + ' packaged-session protocol checks passed; engine services are fixtures.');
