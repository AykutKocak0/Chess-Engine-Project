extends Node2D

const COLOR_LIGHT = Color("#ebecd0")
const COLOR_DARK = Color("#779556")
const SQUARE_SIZE = 75
const BOARD_OFFSET = Vector2(60, 100)

@onready var grid_container = $GridContainer
@onready var eval_label = $EvalLabel

var motor_pid: int = -1
var motor_pipes: Dictionary = {}
var move_history: Array[String] = []
var pending_move: String = "" 
var is_waiting_for_legal_check: bool = false
var output_buffer: String = ""

var starting_board = [
	["bR", "bN", "bB", "bQ", "bK", "bB", "bN", "bR"],
	["bP", "bP", "bP", "bP", "bP", "bP", "bP", "bP"],
	["",   "",   "",   "",   "",   "",   "",   ""],
	["",   "",   "",   "",   "",   "",   "",   ""],
	["",   "",   "",   "",   "",   "",   "",   ""],
	["",   "",   "",   "",   "",   "",   "",   ""],
	["wP", "wP", "wP", "wP", "wP", "wP", "wP", "wP"],
	["wR", "wN", "wB", "wQ", "wK", "wB", "wN", "wR"]
]

var dragging_piece: Sprite2D = null
var original_square_pos = Vector2.ZERO
var original_global_pos = Vector2.ZERO
var is_promoting: bool = false
var en_passant_target_notation: String = "" 
var promotion_from_pos: Vector2 = Vector2.ZERO
var promotion_to_pos: Vector2 = Vector2.ZERO
var promotion_popup: HBoxContainer = null
var is_board_flipped: bool = false 
var depth_spinbox: SpinBox = null
var active_promotion_piece: String = "q"
var is_debug_sandbox_mode: bool = false

func _ready():
	DisplayServer.window_set_size(Vector2i(1280, 800))
	
	var menu_container = VBoxContainer.new()
	menu_container.position = Vector2(850, 150)
	menu_container.add_theme_constant_override("separation", 15)
	add_child(menu_container)
	
	var level_container = HBoxContainer.new()
	level_container.add_theme_constant_override("separation", 10)
	
	var level_label = Label.new()
	level_label.text = "Max Depth:"
	level_container.add_child(level_label)
	
	depth_spinbox = SpinBox.new()
	depth_spinbox.min_value = 1
	depth_spinbox.max_value = 14
	depth_spinbox.value = 6
	depth_spinbox.step = 1
	depth_spinbox.custom_minimum_size = Vector2(100, 35)
	level_container.add_child(depth_spinbox)
	menu_container.add_child(level_container)
	
	var mode_button = Button.new()
	mode_button.text = "Mod: İnsan vs Bot" if not is_debug_sandbox_mode else "Mod: İnsan vs İnsan"
	mode_button.custom_minimum_size = Vector2(200, 45)
	mode_button.pressed.connect(func():
		is_debug_sandbox_mode = !is_debug_sandbox_mode
		mode_button.text = "Mod: İnsan vs Bot" if not is_debug_sandbox_mode else "Mod: İnsan vs İnsan"
		print("[Sistem]: Oyun modu değiştirildi! Sandbox: ", is_debug_sandbox_mode)
		if not is_debug_sandbox_mode:
			var is_white_turn = (move_history.size() % 2 == 0)
			var bot_color = "b" if not is_board_flipped else "w"
			var is_bot_turn = (is_white_turn and bot_color == "w") or (not is_white_turn and bot_color == "b")
			if is_bot_turn:
				trigger_engine_search()
	)
	menu_container.add_child(mode_button)
	
	var flip_button = Button.new()
	flip_button.text = "Tahtayı Çevir (Flip)"
	flip_button.custom_minimum_size = Vector2(200, 45)
	flip_button.pressed.connect(self.flip_board)
	menu_container.add_child(flip_button)
	
	var restart_button = Button.new()
	restart_button.text = "Yeni Oyun (Restart)"
	restart_button.custom_minimum_size = Vector2(200, 45)
	restart_button.pressed.connect(self.restart_game)
	menu_container.add_child(restart_button)

	var promo_label = Label.new()
	promo_label.text = "Piyon Terfi Tercihi:"
	menu_container.add_child(promo_label)
	
	var promo_hbox = HBoxContainer.new()
	promo_hbox.add_theme_constant_override("separation", 10)
	menu_container.add_child(promo_hbox)
	
	var human_color = "b" if is_board_flipped else "w"
	var promo_options = {"q": human_color + "Q", "r": human_color + "R", "b": human_color + "B", "n": human_color + "N"}
	
	for key in promo_options:
		var btn = TextureButton.new()
		var path = "res://" + promo_options[key] + ".svg"
		if ResourceLoader.exists(path):
			btn.texture_normal = load(path)
			btn.custom_minimum_size = Vector2(45, 45)
			btn.ignore_texture_size = true
			btn.stretch_mode = TextureButton.STRETCH_SCALE
			btn.mouse_filter = Control.MOUSE_FILTER_STOP
			
			if key == active_promotion_piece:
				btn.modulate = Color(1, 1, 1, 1)
			else:
				btn.modulate = Color(1, 1, 1, 0.4)
				
			btn.pressed.connect(func():
				active_promotion_piece = key
				for b in promo_hbox.get_children():
					b.modulate = Color(1, 1, 1, 0.4)
				btn.modulate = Color(1, 1, 1, 1)
				print("[Sistem]: Terfi tercihi değiştirildi: ", key.to_upper())
			)
			promo_hbox.add_child(btn)

	if eval_label:
		eval_label.position = Vector2(850, 50) 
		eval_label.text = "0.00"
	
	grid_container.columns = 8
	grid_container.add_theme_constant_override("h_separation", 0)
	grid_container.add_theme_constant_override("v_separation", 0)
	grid_container.position = BOARD_OFFSET
	setup_board()
	start_chess_engine()

func _exit_tree():
	if motor_pid != -1:
		send_to_engine("quit")
		OS.kill(motor_pid)

func start_chess_engine():
	var engine_path = OS.get_executable_path().get_base_dir() + "/Botkut.exe"
	if not FileAccess.file_exists(engine_path):
		engine_path = ProjectSettings.globalize_path("res://Botkut.exe")
		
	if FileAccess.file_exists(engine_path):
		motor_pipes = OS.execute_with_pipe(engine_path, [])
		if motor_pipes.has("stdio"):
			motor_pid = motor_pipes["pid"]
			print("Botkut başarıyla başlatıldı! PID: ", motor_pid)
			send_to_engine("uci")
			send_to_engine("isready")
			send_to_engine("ucinewgame")
	else:
		print("HATA: Botkut.exe proje klasöründe bulunamadı!")

func send_to_engine(command: String):
	if motor_pipes.has("stdio"):
		var pipe = motor_pipes["stdio"]
		pipe.store_line(command)
		pipe.flush() 

func _process(_delta):
	if dragging_piece != null and is_instance_valid(dragging_piece):
		dragging_piece.global_position = get_global_mouse_position()
	check_engine_output()

func check_engine_output():
	if motor_pipes.has("stdio"):
		var pipe = motor_pipes["stdio"]
		if pipe is FileAccess and pipe.is_open():
			while pipe.get_position() < pipe.get_length():
				var character = char(pipe.get_8()) 
				if character == "\n" or character == "\r":
					var final_line = output_buffer.strip_edges()
					output_buffer = "" 
					if final_line != "":
						process_engine_line(final_line)
				else:
					output_buffer += character

func process_engine_line(line: String):
	print("[Botkut]: ", line)
	
	if line.begins_with("info ") and "score" in line:
		var parts = line.split(" ")
		if "cp" in parts:
			var eval_cp_idx = parts.find("cp")
			if eval_cp_idx != -1 and eval_cp_idx + 1 < parts.size():
				var cp_value = float(parts[eval_cp_idx + 1])
				if move_history.size() % 2 != 0:
					cp_value = -cp_value
				var pawn_advantage = cp_value / 100.0
				if abs(pawn_advantage) >= 800.0:
					var mate_in_moves = round((900.0 - abs(pawn_advantage)) * 100.0)
					if mate_in_moves <= 0: mate_in_moves = 1
					if pawn_advantage > 0:
						eval_label.text = "M" + str(mate_in_moves)
						eval_label.modulate = Color("#32ff7e")
					else:
						eval_label.text = "-M" + str(mate_in_moves)
						eval_label.modulate = Color("#ff6b6b")
				else:
					if pawn_advantage > 0:
						eval_label.text = "+" + "%.2f" % pawn_advantage
						eval_label.modulate = Color.WHITE
					elif pawn_advantage < 0:
						eval_label.text = "-" + "%.2f" % abs(pawn_advantage)
						eval_label.modulate = Color("#ff6b6b")
					else:
						eval_label.text = "0.00"
						eval_label.modulate = Color.GRAY
		elif "mate" in parts:
			var mate_index = parts.find("mate")
			if mate_index != -1 and mate_index + 1 < parts.size():
				eval_label.text = "M" + str(parts[mate_index + 1])
				eval_label.modulate = Color.RED

	elif line.begins_with("check_result"):
		var parts = line.split(" ")
		var result = parts[1]
		
		if result == "legal" and is_waiting_for_legal_check and is_instance_valid(dragging_piece):
			var my_piece_name = dragging_piece.get_meta("piece_name") if dragging_piece.has_meta("piece_name") else ""
			var from_pos = basic_notation_to_pos(pending_move.substr(0, 2))
			var to_pos = basic_notation_to_pos(pending_move.substr(2, 2))
			var target_square = grid_container.get_child(to_pos.y * 8 + to_pos.x)
			
			if my_piece_name.ends_with("P") and from_pos.x != to_pos.x:
				var has_target_piece = false
				for child in target_square.get_children():
					if child is Sprite2D:
						has_target_piece = true
						break
				if not has_target_piece:
					var victim_row: int
					if my_piece_name.begins_with("w"):
						victim_row = to_pos.y + 1 
					else:
						victim_row = to_pos.y - 1 
					
					if victim_row >= 0 and victim_row < 8:
						var victim_square = grid_container.get_child(victim_row * 8 + to_pos.x)
						for child in victim_square.get_children():
							if child is Sprite2D:
								child.queue_free()
								print("[Sistem]: İnsan oyuncunun En Passant hamlesiyle rakip piyon silindi.")
			
			for child in target_square.get_children():
				if child is Sprite2D:
					child.queue_free()
			
			dragging_piece.get_parent().remove_child(dragging_piece)
			target_square.add_child(dragging_piece)
			dragging_piece.position = Vector2(SQUARE_SIZE / 2, SQUARE_SIZE / 2)
			
			if my_piece_name == "wK" or my_piece_name == "bK":
				if pending_move == "e1g1": move_rook_manually("h1", "f1")
				elif pending_move == "e8g8": move_rook_manually("h8", "f8")
				elif pending_move == "e1c1": move_rook_manually("a1", "d1")
				elif pending_move == "e8c8": move_rook_manually("a8", "d8")
			
			if pending_move.length() == 5: 
				var promote_code = pending_move.substr(4, 1)
				var actual_pawn_color = my_piece_name.substr(0, 1)
				var new_piece_name = actual_pawn_color + promote_code.to_upper()
				dragging_piece.queue_free()
				var new_sprite = Sprite2D.new()
				new_sprite.texture = load("res://" + new_piece_name + ".svg")
				new_sprite.position = Vector2(SQUARE_SIZE / 2, SQUARE_SIZE / 2)
				new_sprite.scale = Vector2(SQUARE_SIZE / 64.0, SQUARE_SIZE / 64.0)
				new_sprite.set_meta("piece_name", new_piece_name)
				new_sprite.centered = true
				new_sprite.rotation = PI if is_board_flipped else 0.0
				target_square.add_child(new_sprite)
			
			move_history.append(pending_move)
			is_waiting_for_legal_check = false
			dragging_piece = null
			pending_move = ""
			trigger_engine_search()
			
		elif result == "illegal":
			if is_instance_valid(dragging_piece):
				var original_square = grid_container.get_child(original_square_pos.y * 8 + original_square_pos.x)
				if dragging_piece.get_parent() != original_square:
					dragging_piece.get_parent().remove_child(dragging_piece)
					original_square.add_child(dragging_piece)
				dragging_piece.position = Vector2(SQUARE_SIZE / 2, SQUARE_SIZE / 2)
			is_waiting_for_legal_check = false
			dragging_piece = null
			pending_move = ""
			print("UYARI: İllegal hamle reddedildi!")

	elif line.begins_with("bestmove"):
		var parts = line.split(" ")
		if parts.size() > 1 and parts[1] != "(none)":
			if is_debug_sandbox_mode:
				print("[Sandbox - Analiz]: Motorun bu konum için önerdiği hamle: ", parts[1])
			else:
				make_engine_move(parts[1])

func pos_to_notation(pos: Vector2) -> String:
	var files = ["a", "b", "c", "d", "e", "f", "g", "h"]
	var col_index = int(pos.x)
	var file_letter = files[col_index]
	var rank_number: int
	rank_number = 8 - int(pos.y)
	return file_letter + str(rank_number)

func basic_notation_to_pos(note: String) -> Vector2:
	var files = ["a", "b", "c", "d", "e", "f", "g", "h"]
	var ranks = ["8", "7", "6", "5", "4", "3", "2", "1"]
	var col = files.find(note.substr(0, 1))
	var row = ranks.find(note.substr(1, 1))
	return Vector2(col, row)

func _input(event):
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
		if event.pressed:
			var local_pos = grid_container.get_local_mouse_position()
			var col = floor(local_pos.x / SQUARE_SIZE)
			var row = floor(local_pos.y / SQUARE_SIZE)
			
			if col >= 0 and col < 8 and row >= 0 and row < 8:
				var square_index = row * 8 + col
				var square = grid_container.get_child(square_index)
				
				for child in square.get_children():
					if child is Sprite2D:
						var piece_name = child.get_meta("piece_name") if child.has_meta("piece_name") else ""
						var is_white_turn = (move_history.size() % 2 == 0)
						
						if is_debug_sandbox_mode:
							var expected_color = "w" if is_white_turn else "b"
							if not piece_name.begins_with(expected_color):
								print("[Sandbox]: Şu an hamle sırası ", "Beyazda!" if is_white_turn else "Siyahta!")
								return
						else:
							var human_color = "b" if is_board_flipped else "w"
							var is_my_turn = (is_white_turn and human_color == "w") or (not is_white_turn and human_color == "b")
					
							if not is_my_turn or not piece_name.begins_with(human_color):
								print("[Sistem]: Şu an sizin sıranız değil veya rakip taşı seçtiniz!")
								return

						dragging_piece = child
						original_square_pos = Vector2(col, row)
						original_global_pos = dragging_piece.global_position
						dragging_piece.z_index = 100
						break

		elif not event.pressed and is_instance_valid(dragging_piece):
			dragging_piece.z_index = 0
			
			var local_pos = grid_container.get_local_mouse_position()
			var new_col = floor(local_pos.x / SQUARE_SIZE)
			var new_row = floor(local_pos.y / SQUARE_SIZE)
			
			if new_col >= 0 and new_col < 8 and new_row >= 0 and new_row < 8 and (new_col != original_square_pos.x or new_row != original_square_pos.y):
				var piece_name = dragging_piece.get_meta("piece_name") if dragging_piece.has_meta("piece_name") else ""
				var uci_move = pos_to_notation(original_square_pos) + pos_to_notation(Vector2(new_col, new_row))
				
				if piece_name.ends_with("P") and (new_row == 0 or new_row == 7):
					promotion_from_pos = original_square_pos
					promotion_to_pos = Vector2(new_col, new_row)
					uci_move += active_promotion_piece 
				
				print("Orijinal Square Pozisyonu: ", original_square_pos)
				print("Yeni Pozisyon ", Vector2(new_col, new_row))
				pending_move = uci_move
				is_waiting_for_legal_check = true
				print("Tahta Ters mi ", is_board_flipped)
				print("[Sistem] Gönderilen UCI Hamlesi: ", uci_move)
				
				var moves_str = " ".join(move_history)
				send_to_engine("position startpos moves " + moves_str)
				send_to_engine("checklegal " + uci_move)
				dragging_piece.global_position = get_global_mouse_position()
			else:
				dragging_piece.global_position = original_global_pos
				dragging_piece = null

func trigger_engine_search():
	var current_depth = 6
	if depth_spinbox != null:
		current_depth = int(depth_spinbox.value)
		
	var moves_str = " ".join(move_history)
	send_to_engine("position startpos moves " + moves_str)
	send_to_engine("go depth " + str(current_depth)+" wtime 4000 btime 4000 movestogo 1") 

func make_engine_move(uci_move: String):
	if uci_move.length() < 4: return
	
	var from_pos = basic_notation_to_pos(uci_move.substr(0, 2))
	var to_pos = basic_notation_to_pos(uci_move.substr(2, 2))
	var from_square = grid_container.get_child(from_pos.y * 8 + from_pos.x)
	var to_square = grid_container.get_child(to_pos.y * 8 + to_pos.x)
	
	var piece_to_move: Sprite2D = null
	for child in from_square.get_children():
		if child is Sprite2D:
			piece_to_move = child
			break
			
	if piece_to_move:
		var piece_name = piece_to_move.get_meta("piece_name") if piece_to_move.has_meta("piece_name") else ""
		
		if piece_name.ends_with("P") and from_pos.x != to_pos.x:
			var has_target_piece = false
			for child in to_square.get_children():
				if child is Sprite2D:
					has_target_piece = true
					break
			
			if not has_target_piece:
				var victim_row: int
				if piece_name.begins_with("w"):
					victim_row = to_pos.y + 1 if not is_board_flipped else to_pos.y - 1
				else:
					victim_row = to_pos.y - 1 if not is_board_flipped else to_pos.y + 1
				
				if victim_row >= 0 and victim_row < 8:
					var victim_square = grid_container.get_child(victim_row * 8 + to_pos.x)
					for child in victim_square.get_children():
						if child is Sprite2D:
							child.queue_free()
							print("[Sistem]: Motorun En Passant hamlesiyle piyon başarıyla silindi.")
		
		if piece_name == "wK" or piece_name == "bK": 
			if uci_move.substr(0,4) == "e1g1": move_rook_manually("h1", "f1")
			elif uci_move.substr(0,4) == "e8g8": move_rook_manually("h8", "f8")
			elif uci_move.substr(0,4) == "e1c1": move_rook_manually("a1", "d1")
			elif uci_move.substr(0,4) == "e8c8": move_rook_manually("a8", "d8")

		for child in to_square.get_children():
			if child is Sprite2D:
				child.queue_free()
				
		from_square.remove_child(piece_to_move)
		to_square.add_child(piece_to_move)
		piece_to_move.position = Vector2(SQUARE_SIZE / 2, SQUARE_SIZE / 2)
		
		if uci_move.length() == 5:
			var promote_code = uci_move.substr(4, 1)
			var engine_color = "b" if piece_name.begins_with("b") else "w"
			var new_piece_name = engine_color + promote_code.to_upper()
			piece_to_move.queue_free()
			var new_sprite = Sprite2D.new()
			new_sprite.texture = load("res://" + new_piece_name + ".svg")
			new_sprite.position = Vector2(SQUARE_SIZE / 2, SQUARE_SIZE / 2)
			new_sprite.scale = Vector2(SQUARE_SIZE / 64.0, SQUARE_SIZE / 64.0)
			new_sprite.set_meta("piece_name", new_piece_name)
			new_sprite.centered = true
			new_sprite.rotation = PI if is_board_flipped else 0.0
			to_square.add_child(new_sprite)
		
		move_history.append(uci_move)

func move_rook_manually(from_square_note: String, to_square_note: String):
	var from_pos = basic_notation_to_pos(from_square_note)
	var to_pos = basic_notation_to_pos(to_square_note)
	var from_sq = grid_container.get_child(from_pos.y * 8 + from_pos.x)
	var to_sq = grid_container.get_child(to_pos.y * 8 + to_pos.x)
	for child in from_sq.get_children():
		if child is Sprite2D:
			from_sq.remove_child(child)
			to_sq.add_child(child)
			child.position = Vector2(SQUARE_SIZE / 2, SQUARE_SIZE / 2)
			break

func setup_board():
	for child in grid_container.get_children():
		child.queue_free()
	for row in range(8):
		for col in range(8):
			var square = ColorRect.new()
			square.custom_minimum_size = Vector2(SQUARE_SIZE, SQUARE_SIZE)
			square.color = COLOR_LIGHT if (row + col) % 2 == 0 else COLOR_DARK
			grid_container.add_child(square)
			var piece_name = starting_board[row][col]
			if piece_name != "":
				var piece_sprite = Sprite2D.new()
				var path = "res://" + piece_name + ".svg"
				if ResourceLoader.exists(path):
					piece_sprite.texture = load(path)
					piece_sprite.position = Vector2(SQUARE_SIZE / 2, SQUARE_SIZE / 2)
					piece_sprite.scale = Vector2(SQUARE_SIZE / 64.0, SQUARE_SIZE / 64.0)
					piece_sprite.set_meta("piece_name", piece_name)
					piece_sprite.centered = true
					piece_sprite.rotation = PI if is_board_flipped else 0.0
					square.add_child(piece_sprite)

func flip_board():
	is_board_flipped = !is_board_flipped
	grid_container.pivot_offset = Vector2(SQUARE_SIZE * 4, SQUARE_SIZE * 4)
	grid_container.rotation = PI if is_board_flipped else 0.0
	for square in grid_container.get_children():
		for child in square.get_children():
			if child is Sprite2D:
				child.centered = true
				child.rotation = PI if is_board_flipped else 0.0
				
	var menu_container : VBoxContainer = null
	for child in get_children():
		if child is VBoxContainer:
			menu_container = child
			break
			
	if menu_container:
		var promo_hbox : HBoxContainer = null
		for child in menu_container.get_children():
			if child is HBoxContainer and child != menu_container.get_child(0):
				promo_hbox = child
				break
				
		if promo_hbox:
			var human_color = "b" if is_board_flipped else "w"
			var promo_options = {"q": human_color + "Q", "r": human_color + "R", "b": human_color + "B", "n": human_color + "N"}
			var buttons = promo_hbox.get_children()
			var keys = ["q", "r", "b", "n"]
			
			for i in range(buttons.size()):
				if buttons[i] is TextureButton:
					var key = keys[i]
					var path = "res://" + promo_options[key] + ".svg"
					if ResourceLoader.exists(path):
						buttons[i].texture_normal = load(path)
				
	if is_board_flipped and move_history.size() == 0 and not is_waiting_for_legal_check:
		print("[Sistem]: Oyuncu Siyah oldu, Botkut Beyazlar ile açılış hamlesini düşünüyor...")
		trigger_engine_search()

func restart_game():
	print("[Sistem]: Oyun yeniden başlatılıyor...")
	move_history.clear()
	en_passant_target_notation = ""
	is_waiting_for_legal_check = false
	dragging_piece = null
	pending_move = ""
	setup_board()
	send_to_engine("ucinewgame")
	send_to_engine("position startpos")
	eval_label.text = "0.00"
	eval_label.modulate = Color.GRAY
	if is_board_flipped:
		trigger_engine_search()
