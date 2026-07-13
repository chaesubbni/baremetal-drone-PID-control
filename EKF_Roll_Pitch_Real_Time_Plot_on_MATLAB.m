% MATLAB UART Real-time Plot (Roll & Pitch)
% 캘리브레이션 문자열 무시 및 숫자 데이터 파싱 적용 버전

% 시리얼 포트 및 통신 설정
port = "COM5";        
baudrate = 57600;    
s = serialport(port, baudrate);
configureTerminator(s, "LF"); % MCU에서 보내는 '\n'을 데이터의 끝으로 인식

% 실시간 그래프 창 설정
fig = figure('Name', 'Roll & Pitch 실시간 모니터링', 'NumberTitle', 'off');
ax = axes(fig); % 축 객체를 명시적으로 생성 (텍스트 및 Y축 제어용)

% MaximumNumPoints를 설정해 메모리 누수 방지 (화면에 최근 1000개 데이터만 유지)
h_roll = animatedline(ax, 'Color', 'r', 'LineWidth', 1.5, 'DisplayName', 'Roll', 'MaximumNumPoints', 1000);
h_pitch = animatedline(ax, 'Color', 'b', 'LineWidth', 1.5, 'DisplayName', 'Pitch', 'MaximumNumPoints', 1000);

% Y축 범위를 -50에서 50으로 고정
ylim(ax, [-50 50]); 
legend(ax, 'Location', 'northwest'); % 텍스트와 겹치지 않게 범례 위치 조정
title(ax, 'Real-time Roll and Pitch via UART');
xlabel(ax, 'Samples');
ylabel(ax, 'Angle (degrees)');
grid(ax, 'on');

% 우측 상단에 Roll/Pitch 값을 표시할 텍스트 박스 생성 (창 크기가 변해도 위치 유지)
t_val = text(ax, 0.95, 0.95, '', 'Units', 'normalized', ...
    'HorizontalAlignment', 'right', 'VerticalAlignment', 'top', ...
    'FontSize', 12, 'FontWeight', 'bold', 'BackgroundColor', 'w', 'EdgeColor', 'k');

% 데이터 수신 및 실시간 그리기 루프
count = 1;
disp('데이터 수신을 시작합니다. 창을 닫으면 종료됩니다.');
disp('캘리브레이션 메시지 대기 중...');

% Figure 창이 살아있는 동안(X를 눌러 닫기 전까지) 무한 루프
while ishandle(fig) 
    try
        % 통신 버퍼에서 데이터 한 줄('\n' 기준) 읽어오기
        data_str = readline(s);
        
        % ,를 기준으로 문자열을 쪼개고 숫자로 변환
        data_num = str2double(split(data_str, ","));
        
        % 데이터가 정확히 2개이고, 모두 정상적인 숫자(NaN이 아님)일 때만 그래프 업데이트
        if length(data_num) == 2 && ~any(isnan(data_num))
            roll = data_num(1);
            pitch = data_num(2);
            
            % 선에 새로운 데이터 포인트 추가
            addpoints(h_roll, count, roll);
            addpoints(h_pitch, count, pitch);
            
            % 화면 우측 상단 텍스트 업데이트
            t_val.String = sprintf('Roll: %.2f°\nPitch: %.2f°', roll, pitch);
            
            % 화면 업데이트
            drawnow limitrate; 
            
            count = count + 1;
        else
            % 숫자 형태가 아닌 문자열(캘리브레이션 메시지 등) 처리
            % 불필요한 공백이나 줄바꿈을 제거하고 출력
            clean_str = strip(data_str);
            if strlength(clean_str) > 0
                disp("MCU 메시지: " + clean_str);
            end
        end
    catch
        % 통신 중 노이즈로 인해 쓰레기 값이 들어오더라도 프로그램이 뻗지 않게 예외 처리
    end
end

% 4. 안전하게 포트 닫기
clear s;
disp('시리얼 포트를 닫았습니다.');