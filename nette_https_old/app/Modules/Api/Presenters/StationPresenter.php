<?php

declare(strict_types=1);

namespace App\Presenters;

use Tracy;
use Nette;
use Nette\Application\IPresenter;
use Nette\Application\Responses;
use Nette\Application\Request;
use Nette\Utils\DateTime;

final class StationPresenter implements IPresenter
{
    private $database;

    public function __construct(Nette\Database\Context $database)
    {
        $this->database = $database;
    }

    public function run(Request $request): Nette\Application\IResponse
    {
        $action = $request->getParameter('action');
        switch ($action) {
            case "getUsers":
                return new Responses\JsonResponse($this->getUsers($request));
            case "saveAccess":
                return new Responses\JsonResponse($this->saveAccess($request));
            case "saveTemp":
                return new Responses\JsonResponse($this->saveTemp($request));
            default:
                return new Responses\JsonResponse(["status" => "err", "error" => "Empty or invalid request!"]);
        }
    }

    private function saveTemp(Request $request)
    {
        $id_sensor = $request->getParameter('id_sensor');

        if((empty($id_sensor) && $id_sensor!=="0") || ctype_digit($id_sensor)==false)
        {
            return ["status" => "err", "error" => "Empty or invalid request!"];
        }
        

    }

    private function saveAccess(Request $request)
    {
        $id_station = $request->getParameter('id_station');
        $user_rfid = $request->getParameter('user_rfid');
        $status = $request->getParameter('status');
        
        if(empty($id_station) || empty($user_rfid) || (empty($status) && $status!=="0") || ctype_digit($id_station)==false || ctype_digit($status)==false)
        {
            return ["status" => "err", "error" => "Empty or invalid request!"];
        }
        Tracy\Debugger::barDump("C1");
        //check existing station and user
        $row = $this->database->table('stations')->where("id_station = ?", $id_station)->fetch();

        if (!$row) {
            return ["status" => "err", "error" => "Station doesnt exist!"];
        }

        $row = $this->database->table('users')->where("user_rfid = ?", $user_rfid)->fetch();

        if (!$row) {
            return ["status" => "err", "error" => "User doesnt exist!"];
        }

        $result=$this->database->table('access_log')->insert([
            "datetime"=>new DateTime,
            "user_rfid"=>$user_rfid,
            "status"=>$status,
            "id_station"=>$id_station
            ]);
        if(!$result)
        {
            return ["status" => "err", "error" => "Error while saving in database!"];
        }
        
        return ["status" => "ok"];

    }

    private function getUsers(Request $request)
    {
        $id_station = $request->getParameter('id_station');

        if (empty($id_station) || ctype_digit($id_station)==false) {
            return ["status" => "err", "error" => "Id of station not specified or in bad format!"];
        }
        $stations = $this->database->table('stations');
        $row = $stations->where("id_station = ?", $id_station)->fetch();

        if (!$row) {
            return ["status" => "err", "error" => "Id of station not exists!"];
        }

        $row = $this->database->table('stations_users')->where("id_station = ?", $id_station);

        if (!$row) {
            return ["status" => "ok", "users" => ""];
        }
        $response = ["status" => "ok", "users" => array()];
        $count=0;
        foreach ($row as $value) {
            $user = $this->database->table('users')->where("id_user = ?", $value["id_user"])->fetch();
            if (!$user) {
                continue;
            }
            if($value["perm"]==2 && $user["pin"]!="")
            {
                array_push($response["users"], ["rfid" => $user["user_rfid"], "perm" => $value["perm"], "pin"=> $user["pin"]]);
            }
            else if($value["perm"]==1)
            {
                array_push($response["users"], ["rfid" => $user["user_rfid"], "perm" => $value["perm"]]);
            }
            
            $count++;
        }
        $response["count"]=(String)$count;

        $this->database->table('stations')->where("id_station = ?", $id_station)->update(["last_update"=>new Datetime]);
        //Tracy\Debugger::dump($response);
        //return new Responses\TextResponse("");
        return $response;
    }
}
